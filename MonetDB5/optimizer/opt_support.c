/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2011 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * @f opt_support
 * @a M. Kersten
 * @* The MAL Optimizer
 * One of the prime reasons to design the MAL intermediate language is to have
 * a high-level description for database queries, which is easy to generate by
 * a front-end compiler and easy to decode, optimize and interpret.
 *
 * An optimizer needs several mechanisms to be effective. It should be able to
 * perform a symbolic evaluation of a code fragment and collect the result in
 * properties for further decision making. The prototypical case is where an
 * optimizer estimates the result size of a selection.
 *
 * Another major issue is to be able to generate and explore a space of
 * alternative evaluation plans. This exploration may take place up front,
 * but can also be ran at runtime for query fragments.
 *
 * @menu
 * * Landscape::
 * * Dependencies::
 * * Building Blocks::
 * * Framework::
 * * Lifespan Analysis::
 * * Flow Analysis::
 * * Optimizer Toolkit::
 * @end menu
 *
 * @node Landscape, Dependencies ,The MAL Optimizer, The MAL Optimizer
 * @+ The Optimizer Landscape
 * A query optimizer is often a large and complex piece of code, which
 * enumerates alternative evaluation plans from which 'the best' plan
 * is selected for evaluation. Limited progress has been made sofar to
 * decompose the optimizer into (orthogonal) components, because it is
 * a common believe in research that a holistic view on the problem is
 * a prerequisite to find the best plan.
 * Conversely, commercial optimizers use a cost-model driven approach, which
 * explores part of the space using a limited (up to 300) rewriting rules.
 *
 * Our hypothesis is that query optimization should be realized with
 * a collection of query optimizer transformers (QOT),
 * each dedicated to a specific task.
 * Furthermore, they are assembled in scenarios to support specific application
 * domains or achieve a desired behavior. Such scenarios are selected on a
 * session basis, a query basis, or dynamically at
 * runtime; they are part of the query plan.
 *
 * The query transformer list below is under consideration for development.
 * For each we consider its goal, approach, and expected impact.
 * Moreover, the minimal prerequisites identify the essential optimizers that
 * should have done their work already. For example, it doesn't make sense
 * to perform a static evaluation unless you have already propagated the
 * constants using Alias Removal.
 *
 * @emph{Constant expressions}
 * Goal: to remove scalar expressions which need be evaluated once during the
 * query lifetime.
 * Rationale: static expressions appear
 * when variables used denote literal constants (e.g. 1+1),
 * when catalog information can be merged with the plan (e.g. max(B.salary)),
 * when session variables are used which are initialized once (e.g. user()).
 * Early evaluation aids subsequent optimization.
 * Approach: inspect all instructions to locate static expressions.
 * Whether they should be removed depends on the expected re-use,
 * which in most cases call for an explicit request upon query registration
 * to do so. The result of a static evaluation provides a ground for
 * alias removal.
 * Impact: relevant for stored queries (MAL functions)
 * Prereq: alias removal, common terms
 *
 * @emph{Relational Expression Optimizer}
 * Goal: to evaluate a relational plan using properties of BATs,
 * such as being empty or forming an aligned group.
 * These optimizations assume that the code generator can detect properties
 * while compiling e.g. an SQL query.
 * Impact: high
 * Prereq:
 *
 * @emph{Alias Removal }
 * Goal: to reduce the number of  variables referenceing the same value,
 * thereby reducing the analysis complexity.
 * Rationale: query transformations often result in replacing
 * the right-hand side expression with a result variable. This pollutes the
 * code block with simple assignments e.g. V:=T. Within the descendant flow the
 * occurrence of V could be replaced by T, provided V is never assigned a new
 * value.
 * Approach: literal constants within a MAL block are already recognized and
 * replaced by a single variable.
 * Impact: medium.
 *
 *
 * @emph{Common Term Optimizer }
 * Goal: to reduce the amount of work by avoiding calculation of the same
 * operation twice.
 * Rationale: to simplify code generation for front-ends, they do not have
 * to remember the subexpressions already evaluated. It is much easier to
 * detect at the MAL level.
 * Approach: simply walk through the instruction sequence and locate identical
 * patterns.  (Enhance is with semantic equivalent instructions)
 * Impact: High
 * Prereq: Alias Removal
 *
 * @emph{Dead Code Removal }
 * Goal: to remove all instructions whose result is not used
 * Rationale: due to sloppy coding or alternative execution paths
 * dead code may appear. Als XML Pathfinder is expected to produce
 * a large number of simple assignments.
 * Approach: Every instruction should produce a value used somewhere else.
 * Impact: low
 *
 * @emph{Heuristic Rule Rewrites }
 * Goal: to reduce the volume as quick as possible.
 * Rationale: most queries are focussed on a small part of the database.
 * To avoid carrying too many intermediates, the selection should be performed as
 * early as possible in the process. This assumes that selectivity factors are
 * known upfront, which in turn depends on histogram of the value distribution.
 * Approach: locate selections and push them back/forth through the flow graph.
 * Impact: high
 *
 * @emph{Join Path Optimizer }
 * Goal: to reduce the volume produced by a join sequence
 * Rationale: join paths are potentially expensive operations. Ideally the join
 * path is evaluated starting at the smallest component, so as to reduce the
 * size of the intermediate results.
 * Approach: to successfully reduce the volume we need to estimate their processing
 * cost. This calls for statistics over the value distribution, in particular,
 * correlation histograms. If statistics are not available upfront, we have to
 * restore to an incremental algorithm, which decides on the steps using the
 * size of the relations.
 * Impact: high
 *
 * @emph{Operator Sort }
 * Goal: to sort the dataflow graph in such a way as to reduce the
 * cost, or to assure locality of access for operands.
 * Rationale: A simple optimizer is to order the instructions for
 * execution by permutation of the query components
 * Approach:
 * Impact:
 *
 * @emph{Singleton Set }
 * Goal: to replace sets that are known to produce precisely
 * one tuple.
 * Rationale: Singleton sets can be represented by value pairs in the
 * MAL program, which reduces to a scalar expression.
 * Approach: Identify a set variable for replacement.
 * Impact:
 *
 * @emph{Range Propagation }
 * Goal: look for constant ranges in select statements and
 * propagate them through the code.
 * Rationale: partitioned tables and views may give rise to
 * expressions that contain multiple selections over the
 * same BAT. If their arguments are constant, the result
 * of such selects can sometimes be predicted, or the
 * multiple selections can be cascaded into a single operation.
 * Impact: high, should be followed by alias removal and dead code removal
 *
 * @emph{Result Cacher }
 * Goal: to reduce the processing cost by keeping track of expensive to
 * compute intermediate results
 * Rationale:
 * Approach: result caching becomes active after an instruction has been
 * evaluated. The result can be cached as long as its underlying operands
 * remain unchanged. Result caching can be made transparent to the user, but
 * affects the other quer optimizers.
 * Impact: high
 *
 * @emph{Vector Execution }
 * Goal: to rewrite a query to use a cache-optimal vector implementation
 * Rationale: processing in the cache is by far the best you can get.
 * However, the operands may far exceed the cache size and should be broken
 * into pieces followed by a staged execution of the fragments involved.
 * Approach: replace the query plan with fragment streamers
 * Impact:
 *
 * @emph{Staged Execution }
 * Goal: to split a query plan into a number of steps, such that the first
 * response set is delivered as quickly as possible. The remainder is only
 * produced upon request.
 * Rationale: interactive queries call for quick response and an indication
 * of the processing time involved to run it too completion.
 * Approach: staged execution can be realized using a fragmentation scheme
 * over the database, e.g. each table is replaced by a union of fragments.
 * This fragmentation could be determined upfront by the user or is derived
 * from the query and database statistics.
 * impact: high
 *
 * @emph{Code Parallizer }
 * Goal: to exploit parallel IO and cpu processing in both SMP and MPP settings.
 * Rationale: throwing more resources to solve a complex query helps, provided
 * it is easy to determine that parallel processing recovers the administrative
 * overhead
 * Approach: every flow path segment can be handled by an independent process thread.
 * Impact: high
 *
 * @emph{Query Evaluation Maps }
 * Goal: to avoid touching any tuple that is not relevant for answering a query.
 * Rationale: the majority of work in solving a query is to disgard tuples of
 * no interest and to find correlated tuples through join conditions. Ideally,
 * the database learns these properties over time and re-organizes (or builts
 * a map) to replace disgarding by map lookup.
 * Approach: piggyback selection and joins as database fragmentation instructions
 * Impact: high
 *
 * @emph{MAL Compiler (tactics)}
 * Goal: to avoid interpretation of functional expressions
 * Rationale: interpretation of arithmetic expressions with an interpreter
 * is always expensive. Replacing a complex arithmetic expressin with a
 * simple dynamically compiled C-functions often pays off. Especially for
 * cached (MAL) queries
 * Approach:
 * Impact: high
 *
 * @emph{Dynamic Query Scheduler (tactics)}
 * Goal: to organize the work in a way so as to optimize resource usage
 * Rationale: straight interpretation of a query plan may not lead to the best
 * use of the underlying resources. For example, the content of the runtime
 * cache may provide an opportunity to safe time by accessing a cached source
 * Approach: query scheduling is the last step before a relation algebra interpreter
 * takes over control. The scheduling step involves a re-ordering of the
 * instructions within the boundaries imposed by the flow graph.
 * impact: medium
 *
 * @emph{Aggregate Groups }
 * Goal: to reduce the cost of computing aggregate expressions over times
 * Rationale: many of our applications call for calculation of aggregates
 * over dynamically defined groupings. They call for lengtly scans and it pays
 * to piggyback all aggregate calculates, leaving their result in the cache for
 * later consumption (eg the optimizers)
 * Approach:
 * Impact: High
 *
 * @emph{Data Cube optimizer}
 * Goal: to recognize data cube operations
 * Rationale:
 * Approach:
 * Impact:
 *
 *
 * @emph{Demand Driven Interpreter (tactics)}
 * Goal: to use the best interpreter and libraries geared at the task at hand
 * Rationale: Interpretation of a query plan can be based on different
 * computational models. A demand driven interpretation starts at the intended
 * output and 'walks' backward through the flow graph to collect the pieces,
 * possibly in a pipelined fashion. (Vulcano model)
 * Approach: merely calls for a different implementation of the core operators
 * Impact: high
 *
 * @emph{Iterator Strength Reduction }
 * Goal: to reduce the cost of iterator execution by moving instructions
 * out of the loop.
 * Rationale: although iteration at the MAL level should be avoided due to
 * the inherent low performance compared to built-in operators, it is not
 * forbidden. In that case we should confine the iterator block to the minimal
 * work needed.
 * Approach: inspect the flowgraph for each iterator and move instructions around.
 * Impact: low
 *
 * @emph{Accumulator Evaluation }
 * Goal: to replace operators with cheaper ones.
 * Rationale: based on the actual state of the computation and the richness of
 * the supporting libraries there may exists alternative routes to solve a query.
 * Approach: Operator rewriting depends on properties. No general technique.
 * The first implementation looks at calculator expressions such as they
 * appear frequently in the RAM compiler.
 * Impact: high
 * Prerequisite: should be called after common term optimizer to avoid clashes.
 * Status: Used in the SQL optimizer.
 *
 * @emph{Code Inliner }
 * Goal: to reduce the calling depth of the interpreter and to obtain
 * a better starting point for code squeezing
 * Rationale: substitution of code blocks (or macro expansion) leads to
 * longer linear code sequences. This provides opportunities for squeezing.
 * Moreover, at runtime building and managing a stackframe is rather expensive.
 * This should be avoided for functions called repeatedly.
 * Impact: medium
 * Status: Used in the SQL optimizer to handle SQL functions.
 *
 * @emph{Code Outliner }
 * Goal: to reduce the program size by replacing a group with a single
 * instruction
 * Rationale: inverse macro expansion leads to
 * shorter linear code sequences. This provides opportunities for less interpreter
 * overhead, and to optimize complex, but repetative instruction sequences
 * with a single hardwired call
 * Approach: called explicitly to outline a module (or symbol)
 * Impact: medium
 *
 *
 * @emph{Garbage Collector }
 * Goal: to release resources as quickly as possible
 * Rationale: BATs referenced from a MAL program keep resources locked.
 * Approach: In cooperation with a resource scheduler we should identify those
 * that can be released quickly. It requires a forced gargabe collection call
 * at the end of the BAT's lifespan.
 * Impact: large
 * Status: Implemented. Algorithm based on end-of-life-span analysis.
 *
 * @emph{Foreign Key replacements }
 * Goal: to improve multi-attribute joins over foreign key constraints
 * Rationale: the code produced by the SQL frontend involves foreign key
 * constraints, which provides many opportunities for speedy code
 * using a join index.
 * Impact: large
 * Status: Implemented in the SQL strategic optimizer.
 *
 * @node Dependencies, Building Blocks, Landscape, The MAL Optimizer
 * @subsection Optimizer Dependencies
 * The optimizers are highly targeted to a particular problem.
 * Aside from the resources available to invest in plan optimization,
 * optimizers are partly dependent and may interfere.
 *
 * To aid selection of the components of interest, we have grouped
 * them in a preferred order of deployment.
 *
 * @multitable @columnfractions .12 .8
 * @item Group A:
 * @tab    Code Inliner.
 * @item
 * @tab    Constant Expression Evaluator.
 * @item
 * @tab    Relational Expression Evaluator.
 * @item
 * @tab    Strength Reduction.
 *
 * @item Group B:
 * @tab    Common Term Optimizer.
 * @item
 * @tab    Query Evaluation Maps.
 *
 * @item Group C:
 * @tab    Join Path Optimizer.
 * @item
 * @tab    Ranges Propagation.
 * @item
 * @tab    Operator Cost Reduction.
 * @item
 * @tab    Operator Sort.
 * @item
 * @tab    Foreign Key handling.
 * @item
 * @tab    Aggregate Groups.
 * @item
 * @tab    Data Cube optimizer.
 * @item
 * @tab    Heuristic Rule Rewrite.
 *
 * @item group D:
 * @tab    Code Parallizer.
 * @item
 * @tab    Accumulator Evaluations.
 * @item
 * @tab    Result Cacher.
 * @item
 * @tab	Replication Manager.
 *
 * @item group E:
 * @tab    MAL Compiler.
 * @item
 * @tab    Dynamic Query Scheduler.
 * @item
 * @tab    Vector Execution.
 * @item
 * @tab    Staged Execution.
 *
 * @item group F:
 * @tab    Alias Removal.
 * @item
 * @tab    Dead Code Removal.
 * @item
 * @tab    Garbage Collector.
 * @end multitable
 * The interaction also excludes combinations. For example,
 * the Accumulator should be used after the Partition
 * optimizer.
 *
 * @node Building Blocks, Framework, Dependencies, The MAL Optimizer
 * @- Optimizer Building Blocks
 *
 * Some instructions are independent of the execution context. In particular,
 * expressions over side-effect free functions with constant parameters could
 * be evaluated before the program block is considered further.
 *
 * A major task for an optimizer is to select instruction (sequences) which
 * can and should be replaced with cheaper ones. The cost model underlying
 * this decision depends on the processing stage and the overall objective.
 * For example, based on a symbolic analysis their may exist better
 * implementations within the interpreter to perform the job (e.g. hashjoin vs
 * mergejoin). Alternative, expensive intermediates may be cached for later use.
 *
 * Plan enumeration is often implemented as a Memo structure, which
 * designates alternative sub-plans based on a cost metric.
 * Perhaps we can combine these memo structures into a large table
 * for all possible combinations encountered for a user.
 *
 * The MAL language does not imply a specific optimizer to be used. Its programs
 * are merely a sequence of specifications, which is interpreted by an engine
 * specific to a given task. Activation of the engine is controlled by a
 * scenario, which currently includes two hooks for optimization; a
 * strategic optimizer and a tactical optimizer.
 * Both engines take a MAL program and produce a (new/modified) MAL program for
 * execution by the lower layers.
 *
 * MAL programs end-up in the symbol table linked to a user session.
 * An optimizer has the freedom to change the code, provided it is known that
 * the plan derived is invariant to changes in the environment.
 * All others lead to alternative plans, which should be collected as a trail of
 * MAL program blocks. These trails can be inspected for a
 * posteriori analysis, at least in terms of some statistics on the properties
 * of the MAL program structures automatically.
 * Alternatively, the trail may be pruned and re-optimized when appropriate
 * from changes in the environment.
 *
 * The rule applied for all optimizers is to not-return
 * before checking the state of the MAL program,
 * and to assure the dataflow and variable scopes are properly set.
 * It costs some performance, but the difficulties
 * that arise from optimizer interference are very hard to debug.
 * One of the easiest pitfalls is to derive an optimized
 * version of a MAL function while it is already referenced by or
 * when polymorphic typechecking is required afterwards.
 *
 * @- Building Your Own Optimizer
 * Implementation of your own MAL-MAL optimizer can best be started
 * from refinement of one of the examples included in the code base.
 * Beware that only those used in the critical path of SQL execution
 * are thorouhly tested. The others are developed up to the point that
 * the concept and approach can be demonstrated.
 *
 * The general structure of most optimizers is to actively copy
 * a MAL block into a new program structure. At each step we
 * determine the action taken, e.g. replace the instruction or
 * inject instructions to achieve the desired goal.
 *
 * A tally on major events should be retained, because it gives
 * valuable insight in the effectiveness of your optimizer.
 * The effects of all optimizers is collected in a system catalog.
 *
 * Each optimizer ends with a strong defense line, @code{optimizerCheck()}
 * It performs a complete type and data flow analysis before returning.
 * Moreover, if you are in debug mode, it will  keep a copy of the
 * plan produced for inspection. Studying the differences between
 * optimizer steps provide valuable information to improve your code.
 *
 * The functionality of the optimizer should be clearly delineated.
 * The guiding policy is that it is always safe to not apply an
 * optimizer step.
 * This helps to keep the optimizers as independent as possible.
 *
 * It really helps if you start with a few tiny examples to test
 * your optimizer. They should be added to the Tests directory
 * and administered in Tests/All.
 *
 * Breaking up the optimizer into different components and
 * grouping them together in arbitrary sequences calls for
 * careful programming.
 *
 * One of the major hurdles is to test interference of the
 * optimizer. The test set is a good starting point, but does
 * not garantee that all cases have been covered.
 *
 * In principle, any subset of optimizers should work flawlessly.
 * With a few tens of optimizers this amounts to potential millions
 * of runs. Adherence to a partial order reduces the problem, but
 * still is likely to be too resource consumptive to test continously.
 *
 * The optimizers defined here are registered to the optimizer module.
 */
/*
 * @-
 * @node Framework, Lifespan Analysis, Building Blocks, The MAL Optimizer
 * @subsection Optimizer framework
 * The large number of query transformers calls for a flexible scheme for
 * the deploy them. The approach taken is to make all optimizers visible
 * at the language level as a signature
 * @code{optimizer.F()} and @code{optimizer.F(mod,fcn)}.
 * The latter designates a target function to be inspected by
 * the optimizer @code{F()}.
 * Then (semantic) optimizer merely
 * inspects a MAL block for their occurrences and activitates it.
 *
 * The optimizer routines have access to the client context, the MAL block,
 * and the program counter where the optimizer call was found. Each optimizer
 * should remove itself from the MAL block.
 *
 * The optimizer repeatedly runs through the program until
 * no optimizer call is found.
 *
 * Note, all optimizer instructions are executed only once. This means that the
 * instruction can be removed from further consideration. However, in the case
 * that a designated function is selected for optimization (e.g.,
 * commonTerms(user,qry)) the pc is assumed 0. The first instruction always
 * denotes the signature and can not be removed.
 *
 * To safeguard against incomplete optimizer implementations it
 * is advisable to perform an optimizerCheck at the end.
 * It takes as arguments the number of optimizer actions taken
 * and the total cpu time spent.
 * The body performs a full flow and type check and re-initializes
 * the lifespan administration. In debugging mode also a copy
 * of the new block is retained for inspection.
 *
 * @node Lifespan Analysis, Flow Analysis, Framework, The MAL Optimizer
 * @subsection Lifespan analysis
 * Optimizers may be interested in the characteristic of the
 * barrier blocks for making a decision.
 * The variables have a lifespan in the code blocks, denoted by properties
 * beginLifespan,endLifespan. The beginLifespan denotes the intruction where
 * it receives its first value, the endLifespan the last instruction in which
 * it was used as operand or target.
 *
 * If, however, the last use lies within a BARRIER block, we can not be sure
 * about its end of life status, because a block redo may implictly
 * revive it. For these situations we associate the endLifespan with
 * the block exit.
 *
 * In many cases, we have to determine if the lifespan interferes with
 * a optimization decision being prepared.
 * The lifespan is calculated once at the beginning of the optimizer sequence.
 * It should either be maintained to reflect the most accurate situation while
 * optimizing the code base. In particular, it means that any move/remove/addition
 * of a MAL instruction calls for either a recalculation or further propagation.
 * Unclear what will be the best strategy. For the time being we just recalc.
 * If one of the optimizers fails, we should not attempt others.
 */
#include "monetdb_config.h"
#include "opt_prelude.h"
#include "opt_support.h"
#include "mal_interpreter.h"
#include "mal_debugger.h"
#include "opt_multiplex.h"

/*
 * @-
 * Optimizer catalog with runtime statistics;
 */
struct OPTcatalog {
	char *name;
	int enabled;
	int calls;
	int actions;
	int debug;
} optcatalog[]= {
{"accessmode",	0,	0,	0,	DEBUG_OPT_ACCESSMODE},
{"accumulators",0,	0,	0,	DEBUG_OPT_ACCUMULATORS},
{"aliases",		0,	0,	0,	DEBUG_OPT_ALIASES},
{"cluster",		0,	0,	0,	DEBUG_OPT_CLUSTER},
{"coercion",	0,	0,	0,	DEBUG_OPT_COERCION},
{"commonTerms",	0,	0,	0,	DEBUG_OPT_COMMONTERMS},
{"compress",	0,	0,	0,	DEBUG_OPT_COMPRESS},
{"constants",	0,	0,	0,	DEBUG_OPT_CONSTANTS},
{"costModel",	0,	0,	0,	DEBUG_OPT_COSTMODEL},
{"crack",		0,	0,	0,	DEBUG_OPT_CRACK},
{"datacell",	0,	0,	0,	DEBUG_OPT_DATACELL},
{"datacyclotron",0,	0,	0,	DEBUG_OPT_DATACYCLOTRON},
{"dataflow",	0,	0,	0,	DEBUG_OPT_DATAFLOW},
{"deadcode",	0,	0,	0,	DEBUG_OPT_DEADCODE},
{"derivePath",	0,	0,	0,	DEBUG_OPT_DERIVEPATH},
{"dictionary",	0,	0,	0,	DEBUG_OPT_DICTIONARY},
{"emptySet",	0,	0,	0,	DEBUG_OPT_EMPTYSET},
{"evaluate",	0,	0,	0,	DEBUG_OPT_EVALUATE},
{"factorize",	0,	0,	0,	DEBUG_OPT_FACTORIZE},
{"garbage",		0,	0,	0,	DEBUG_OPT_GARBAGE},
{"history",		0,	0,	0,	DEBUG_OPT_HISTORY},
{"inline",		0,	0,	0,	DEBUG_OPT_INLINE},
{"joinPath",	0,	0,	0,	DEBUG_OPT_JOINPATH},
{"joinselect",	0,	0,	0,	DEBUG_OPT_JOINSELECT},
{"macro",		0,	0,	0,	DEBUG_OPT_MACRO},
{"mapreduce",	0,	0,	0,	DEBUG_OPT_MAPREDUCE},
{"mergetable",	0,	0,	0,	DEBUG_OPT_MERGETABLE},
{"mitosis",		0,	0,	0,	DEBUG_OPT_MITOSIS},
{"multiplex",	0,	0,	0,	DEBUG_OPT_MULTIPLEX},
{"octopus",		0,	0,	0,	DEBUG_OPT_OCTOPUS},
{"origin",		0,	0,	0,	DEBUG_OPT_ORIGIN},
{"partitions",	0,	0,	0,	DEBUG_OPT_PARTITIONS},
{"peephole",	0,	0,	0,	DEBUG_OPT_PEEPHOLE},
{"prejoin",		0,	0,	0,	DEBUG_OPT_PREJOIN},
{"pushranges",	0,	0,	0,	DEBUG_OPT_PUSHRANGES},
{"recycler",	0,	0,	0,	DEBUG_OPT_RECYCLE},
{"reduce",		0,	0,	0,	DEBUG_OPT_REDUCE},
{"remap",		0,	0,	0,	DEBUG_OPT_REMAP},
{"remote",		0,	0,	0,	DEBUG_OPT_REMOTE},
{"reorder",		0,	0,	0,	DEBUG_OPT_REORDER},
{"replication",	0,	0,	0,	DEBUG_OPT_REPLICATION},
{"selcrack",	0,	0,	0,	DEBUG_OPT_SELCRACK},
{"sidcrack",	0,	0,	0,	DEBUG_OPT_SIDCRACK},
{"singleton",	0,	0,	0,	DEBUG_OPT_SINGLETON},
{"strengthreduction",	0,	0,	0,	DEBUG_OPT_STRENGTHREDUCTION},
{"trace",		0,	0,	0,	DEBUG_OPT_TRACE},
{ 0,	0,	0,	0,	0}
};

lng optDebug;

/*
 * @-
 * Front-ends can set a collection of optimizers by name or their pipe alias.
 */
str
OPTsetDebugStr(int *ret, str *nme)
{
	int i;
	str name= *nme, t, s, env = 0;

	(void) ret;
	optDebug = 0;
	if ( name == 0 || *name == 0)
		return MAL_SUCCEED;
	name = GDKstrdup(name);

	if ( strstr(name,"_pipe") ){
		env = GDKgetenv(name);
		if ( env ) {
			GDKfree(name);
			name = GDKstrdup(env);
		}
	}
	for ( t = s = name; t && *t ; t = s){
		s = strchr(s,',');
		if ( s ) *s++ = 0;
		for ( i=0; optcatalog[i].name; i++)
		if ( strcmp(t,optcatalog[i].name) == 0){
			optDebug |= DEBUG_OPT(optcatalog[i].debug);
			break;
		}
	}
	GDKfree(name);
	return MAL_SUCCEED;
}

str
optimizerCheck(Client cntxt, MalBlkPtr mb, str name, int actions, lng usec, int flag)
{
	if( actions > 0){
		if( flag & OPT_CHECK_TYPES) chkTypes(cntxt->nspace, mb, FALSE);
		if( flag & OPT_CHECK_FLOW) chkFlow(mb);
		if( flag & OPT_CHECK_DECL) chkDeclarations(mb);
	}
	if( cntxt->debugOptimizer){
		/* keep the actions take as post block comments */
		char buf[BUFSIZ];
		sprintf(buf,"%-20s actions=%2d time=" LLFMT " usec",name,actions,usec);
		newComment(mb,buf);
		if (mb->errors)
			throw(MAL, name, PROGRAM_GENERAL);
	}
	/* code to collect all last versions to study code coverage  in SQL
	{stream *fd;
	char nme[25];
	snprintf(nme,25,"/tmp/mal_%d",getpid());
	fd= open_wastream(nme);
	if( fd == NULL)
		printf("Error in %s\n",nme);
	printFunction(fd,mb,0,LIST_MAL_ALL);
	mnstr_close(fd);
	}
	*/
	return MAL_SUCCEED;
}
/*
 * @-
 * Limit the loop count in the optimizer to guard against indefinite
 * recursion, provided the optimizer does not itself generate
 * a growing list.
 */
str
optimizeMALBlock(Client cntxt, MalBlkPtr mb)
{
	InstrPtr p;
	int pc;
	int qot = 0;
	str msg = MAL_SUCCEED;
	int cnt = 0;

	optimizerInit();
	/* assume the type and flow have been checked already */
	/* SQL functions intended to be inlined should not be optimized */
	if ( varGetProp( mb, getArg(getInstrPtr(mb,0),0), inlineProp ) != NULL &&
		 varGetProp( mb, getArg(getInstrPtr(mb,0),0), sqlfunctionProp ) != NULL
	)
        return 0;


	do {
		/* any errors should abort the optimizer */
		if (mb->errors)
			break;
		qot = 0;
		for (pc = 0; pc < mb->stop ; pc++) {
			p = getInstrPtr(mb, pc);
			if (getModuleId(p) == optimizerRef && p->fcn) {
				/* all optimizers should behave like patterns */
				/* However, we don;t have a stack now */
				qot++;
				msg = (str) (*p->fcn) (cntxt, mb, 0, p);
				if (msg) {
					str place = getExceptionPlace(msg);
					msg= createException(getExceptionType(msg), place, "%s", getExceptionMessage(msg));
					GDKfree(place);
					return msg;
				}
				pc= -1;
			}
		}
	} while (qot && cnt++ < mb->stop);
	if (cnt >= mb->stop)
		throw(MAL, "optimizer.MALoptimizer", OPTIMIZER_CYCLE);
	return 0;
}

/*
 * @-
 * The default MAL optimizer includes a final call to
 * the multiplex expander.
 * We should take care of functions marked as 'inline',
 * because they should be kept in raw form.
 * Their optimization takes place after inlining.
 */
str
MALoptimizer(Client c)
{
	str msg;

	if ( varGetProp(c->curprg->def,0,inlineProp))
		return MAL_SUCCEED;
	msg= optimizeMALBlock(c, c->curprg->def);
	if( msg == MAL_SUCCEED)
		OPTmultiplexSimple(c);
	return msg;
}

int hasSameSignature(MalBlkPtr mb, InstrPtr p, InstrPtr q, int stop){
	int i;

	if ( getFunctionId(q) == getFunctionId(p) &&
		 getModuleId(q) == getModuleId(p) &&
		getFunctionId(q) != 0 &&
		getModuleId(q) != 0) {
		if( q->retc != p->retc || q->argc != p->argc) return FALSE;
		assert(stop <= p->argc);
		for( i=0; i<stop; i++)
			if (getArgType(mb,p,i) != getArgType(mb,q,i))
				return FALSE;
		return TRUE;
	}
	return FALSE;
}

int hasSameArguments(MalBlkPtr mb, InstrPtr p, InstrPtr q)
{   int k;
	int (*cmp)(ptr,ptr);
	VarPtr w,u;

	(void) mb;
	if( p->retc != q->retc || p->argc != q->argc)
		return FALSE;
	for(k=p->retc; k<p->argc;k++)
		if( q->argv[k]!= p->argv[k]){
			if( isVarConstant(mb,getArg(p,k)) &&
				isVarConstant(mb,getArg(q,k)) ) {
					w= getVar(mb,getArg(p,k));
					u= getVar(mb,getArg(q,k));
					cmp = BATatoms[w->value.vtype].atomCmp;
					if ( w->value.vtype == u->value.vtype &&
						(*cmp)(VALget(&w->value), VALget(&u->value)) == 0)
						continue;
			}
			return FALSE;
		}
	return TRUE;
}
/*
 * @-
 * If two instructions have elements in common in their target list,
 * it means a variable is re-initialized and should not be considered
 * an alias.
 */
int
hasCommonResults(InstrPtr p, InstrPtr q)
{
	int k, l;

	for (k = 0; k < p->retc; k++)
		for (l = 0; l < q->retc; l++)
			if (p->argv[k] == q->argv[l])
				return TRUE;
	return FALSE;
}
/*
 * @-
 * Dependency between target variables and arguments can be
 * checked with isDependent().
 */
int
isDependent(InstrPtr p, InstrPtr q){
	int i,j;
	for(i= 0; i<q->retc; i++)
	for(j= p->retc; j<p->argc; j++)
		if( getArg(q,i)== getArg(p,j)) return TRUE;
	return FALSE;
}
/*
 * @-
 * See is all arguments mentioned in the instruction at point pc
 * are still visible at instruction qc and have not been updated
 * in the mean time.
 * Take into account that variables may be declared inside
 * a block. This can be calculated using the BARRIER/CATCH
 * and EXIT pairs.
 */
static int
countBlocks(MalBlkPtr mb, int start, int stop){
	int i,cnt =0;
	InstrPtr p;
	for(i= start; i< stop; i++){
		p= getInstrPtr(mb,i);
		if ( p->barrier == BARRIERsymbol || p->token== CATCHsymbol)
			cnt++;
		if ( p->barrier == EXITsymbol )
			cnt--;
	}
	return cnt;
}
#if 0
int
allArgumentsVisible(MalBlkPtr mb, Lifespan span, int pc,int qc){
	int i;
	InstrPtr p;

	if( countBlocks(mb,pc,qc) )
		return FALSE;
	p= getInstrPtr(mb,pc);
	for(i=p->retc; i< p->argc; i++){
		if( getLastUpdate(span,getArg(p,i)) >  getBeginLifespan(span,getArg(p,i)) &&
			qc > getLastUpdate(span,getArg(p,i)) )
			return FALSE;
	}
	return TRUE;
}
#endif
int
allTargetsVisible(MalBlkPtr mb, Lifespan span, int pc,int qc){
	int i;
	InstrPtr p;

	if( countBlocks(mb,pc,qc) )
		return FALSE;
	p= getInstrPtr(mb,pc);
	for(i=0; i < p->retc; i++){
		if( getLastUpdate(span,getArg(p,i))> getBeginLifespan(span,getArg(p,i))  &&
			qc > getLastUpdate(span,getArg(p,i)) )
			return FALSE;
	}
	return TRUE;
}
/*
 * @-
 * The safety property should be relatively easy to determine for
 * each MAL function. This calls for accessing the function MAL block
 * and to inspect the arguments of the signature.
 */
int
isUnsafeFunction(InstrPtr q)
{
	InstrPtr p;

	if (q->fcn == 0 || getFunctionId(q) == 0 || q->blk == NULL)
		return FALSE;
	p= getInstrPtr(q->blk,0);
	if( p->retc== 0)
		return TRUE;
	return (varGetProp( q->blk, getArg(p,0), unsafeProp ) != NULL);
	/* check also arguments for 'unsafe' property */
}

/*
 * @-
 * Instructions are unsafe is one of the arguments is also mentioned
 * in the result list. Alternatively, the 'unsafe' property is set
 * for the function call itself.
 */
int
isUnsafeInstruction(InstrPtr q)
{
	int j, k;

	for (j = 0; j < q->retc; j++)
		for (k = q->retc; k < q->argc; k++)
			if (q->argv[k] == q->argv[j])
				return TRUE;
	return FALSE;
}

/*
 * @-
 * The routine isInvariant determines if the variable V is not
 * changed in the instruction sequence identified by the range [pcf,pcl].
 */
int
isInvariant(MalBlkPtr mb, int pcf, int pcl, int varid)
{
	(void) mb;
	(void) pcf;
	(void) pcl;
	(void) varid;		/*fool compiler */
	return TRUE;
}

/*
 * @-
 * Any instruction may block identification of a common
 * subexpression. It suffices to stumble upon an unsafe function
 * whose parameter lists has a non-empty intersection with the
 * targeted instruction.
 * To illustrate, consider the sequence
 * @example
 * L1 := f(A,B,C);
 * ...
 * G1 := g(D,E,F);
 * ...
 * l2:= f(A,B,C);
 * ...
 * L2:= h()
 * @end example
 *
 * The instruction G1:=g(D,E,F) is blocking if G1 is an alias
 * for @verb{ { }A,B,C@verb{ } }.
 * Alternatively, function g() may be unsafe and @verb{ { }D,E,F@verb{ } }
 * has a non-empty intersection with @verb{ { }A,B,C@verb{ } }.
 * An alias can only be used later on for readonly (and not be used for a function with side effects).
 */
int
safetyBarrier(InstrPtr p, InstrPtr q)
{
	int i,j;
	if( isDependent(q,p))
		return TRUE;
	if (isUnsafeFunction(q)) {
		for (i = p->retc; i < p->argc; i++)
			for (j = q->retc; j < q->argc; j++)
				if (p->argv[i] == q->argv[j]) {
					/* TODO check safety property of the argument */
					return TRUE;
				}
	}
	return FALSE;
}
/*
 * @-
 * Variables can be changed later in the program and, thus, may
 * not be simply replaced by an alias.
 * Variables could also be changed by functions with side effects.
 */
int
isUpdated(MalBlkPtr mb, int pc)
{
	InstrPtr p, q;
	int j, k;

	p = getInstrPtr(mb, pc);
	for (pc++; pc < mb->stop; pc++) {
		q = getInstrPtr(mb, pc);
		/* target is later assigned a new value */
		for (j = 0; j < p->retc; j++)
			for (k = 0; k < q->retc; k++)
				if (p->argv[j] == q->argv[k]) {
					int c = 0;

					if (p->argc != q->argc)
						return TRUE;

					/* instruction q may not be a common expression */
					/* TO WEAK, test stability of its arguments */
					for (j = 0; j < p->argc; j++)
						if (p->argv[j] == q->argv[k] && isInvariant(mb, 0, pc, q->argv[k]))
							c++;
					return c != p->argc;
				}

		/* result is used in an unsafe function */
		for (j = 0; j < p->retc; j++)
			for (k = q->retc; k < q->argc; k++)
				if (p->argv[j] == q->argv[k] ){
				/*
				 * @-
				 * If the operation involves an update of the operand it should told.
				 */
				if ( isUpdateInstruction(q) )
					return TRUE;
				if (getFunctionId(q) && idcmp("destroy", getFunctionId(q)) == 0)
					return TRUE;
			}
	}
	return FALSE;
}

/*
 * @-
 * In many cases we should be assured that a variable is not used in
 * the instruction range identified. For, we may exchange some instructions that
 * might change its content.
 */
#if 0
int
isTouched(MalBlkPtr mb, int varid, int p1, int p2)
{
	int i, k;

	for (i = p1; i < p2; i++) {
		InstrPtr p = getInstrPtr(mb, i);

		for (k = 0; k < p->argc; k++)
			if (p->argv[k] == varid)
				return TRUE;
	}
	return FALSE;
}
#endif

/*
 * @-
 * @node Flow Analysis, Optimizer Toolkit, Lifespan Analysis, The MAL Optimizer
 * @subsection Flow analysis
 * In many optimization rules, the data flow dependency between statements is
 * of crucial importance. The MAL language encodes a multi-source, multi-sink
 * dataflow network. Optimizers typically extract part of the workflow and use
 * the language properties to enumerate semantic equivalent solutions, which
 * under a given cost model turns out to result in better performance.
 *
 * The flow graph plays a crucial role in many optimization steps.
 * It is unclear as yet what primitives and what storage structure is
 * most adequate. For the time being we introduce the operations needed and
 * evaluate them directly against the program
 *
 * For each variable we should determine its scope of stability.
 * End-points in the flow graph are illustrative as dead-code,
 * that do not produce persistent data. It can be removed when
 * you know there are no side-effect.
 *
 * Side-effect free evaluation is a property that should be known upfront.
 * For the time being, we assume it for all operations known to the system.
 * The property "unsafe" is reserved to identify cases where this does not hold.
 * Typically, a bun-insert operation is unsafe, as it changes one of the parameters.
 * @
 * Summarization of the data flow dependencies can be modelled as a dependency graph.
 * It can be made explicit or kept implicit using the operators needed.
 * We start with the latter. The primary steps to deal with is dead code removal.
 * @- Basic Algebraic Blocks
 * Many code snippets produced by e.g. the SQL compiler is just
 * a linear representation of an algebra tree/graph. Its detection
 * makes a number of optimization decisions more easy, because
 * the operations are known to be side-effect free within the tree/graph.
 * This can be used to re-order the plan without concern on impact of the outcome.
 * It suffice to respect the flow graph.
 * [unclear as what we need]
 * @-
 * @node Optimizer Toolkit, Access Mode, Flow Analysis , The MAL Optimizer
 * @+ Optimizer Toolkit
 * In this section, we introduce the collection of MAL optimizers
 * included in the code base. The tool kit is incrementally built, triggered
 * by experimentation and curiousity. Several optimizers require
 * further development to cope with the many features making up the MonetDB system.
 * Such limitations on the implementation are indicated where appropriate.
 *
 * Experience shows that construction and debugging of a front-end specific optimizer
 * is simplified when you retain information on the origin of the MAL code
 * produced as long as possible. For example,
 * the snippet @code{ sql.insert(col, 12@@0, "hello")} can be the target
 * of simple SQL rewrites using the module name as the discriminator.
 *
 * Pipeline validation. The pipelines used to optimize MAL programs contain
 * dependencies. For example, it does not make much sense to call the deadcode
 * optimizer too early in the pipeline, although it is not an error.
 * Moreover, some optimizers are merely examples of the direction to take,
 * others are critical for proper functioning for e.g. SQL.
 *
 * @menu
 * * Access Mode::
 * * Accumulators::
 * * Alias Removal::
 * * Code Factorization::
 * * Coercions::
 * * Common Terms::
 * * Constant Expressions::
 * * Cost Models::
 * * Data Flow::
 * * Dead Code Removal::
 * * Empty Set Removal::
 * * Garbage Collector::
 * * Heuristic Rules::
 * * Inline Functions::
 * * Join Paths::
 * * Macro Processing::
 * * Memo Execution::
 * * Merge Tables ::
 * * Multiplex Compiler::
 * * Partitioned Tables::
 * * Peephole Optimization::
 * * Query Plans::
 * * Range Propagation::
 * * Recycler::
 * * Remote::
 * * Remote Queries::
 * * Singleton Sets ::
 * * Stack Reduction::
 * * Strength Reduction::
 * @end menu
 * @-
 * The dead code remover should not be used for testing,
 * because it will trim most programs to an empty list.
 * The side effect tests should become part of the signature
 * definitions.
 *
 * A side effect is either an action to leave data around
 * in a variable/resource outside the MALblock.
 * A variant encoded here as well is that the linear flow
 * of control can be broken.
 */

int
isProcedure(MalBlkPtr mb, InstrPtr p)
{
	if (p->retc == 0 || (p->retc == 1 && getArgType(mb,p,0) == TYPE_void))
		return TRUE;
/*	if (p->retc == 1 && (varGetProp( q->blk, getArg(p,0), unsafeProp ) != NULL))
		return TRUE; */
	return FALSE;
}

int
isUpdateInstruction(InstrPtr p){
	if ( (getModuleId(p) == batRef || getModuleId(p)==sqlRef) &&
	   (getFunctionId(p) == insertRef ||
		getFunctionId(p) == inplaceRef ||
		getFunctionId(p) == appendRef ||
		getFunctionId(p) == updateRef ||
		getFunctionId(p) == replaceRef ||
		getFunctionId(p) == deleteRef ))
			return TRUE;
	return FALSE;
}
int
hasSideEffects(InstrPtr p, int strict)
{
	if( getFunctionId(p) == NULL) return FALSE;

/*
	if ( getModuleId(p) == algebraRef &&
		 getFunctionId(p) == reuseRef)
			return TRUE;
*/
	if ( (getModuleId(p) == batRef || getModuleId(p)==sqlRef) &&
	     (getFunctionId(p) == setAccessRef ||
	 	  getFunctionId(p) == setWriteModeRef ||
		  getFunctionId(p) == clear_tableRef))
		return TRUE;

	if (getFunctionId(p) == depositRef)
		return TRUE;

	if( getModuleId(p) == ioRef ||
		getModuleId(p) == streamsRef ||
		getModuleId(p) == bstreamRef ||
		getModuleId(p) == mdbRef ||
		getModuleId(p) == bpmRef ||
		getModuleId(p) == malRef ||
		getModuleId(p) == remapRef ||
		getModuleId(p) == constraintsRef ||
		getModuleId(p) == optimizerRef ||
		getModuleId(p) == lockRef ||
		getModuleId(p) == semaRef ||
		getModuleId(p) == recycleRef ||
		getModuleId(p) == alarmRef)
			return TRUE;

	if (getModuleId(p) == sqlRef){
		if (getFunctionId(p) == bindRef) return FALSE;
		if (getFunctionId(p) == bindidxRef) return FALSE;
		if (getFunctionId(p) == binddbatRef) return FALSE;
		if (getFunctionId(p) == columnBindRef) return FALSE;
		if (getFunctionId(p) == copy_fromRef) return FALSE;
		/* assertions are the end-point of a flow path */
		if (getFunctionId(p) == not_uniqueRef) return FALSE;
		if (getFunctionId(p) == zero_or_oneRef) return FALSE;
		if (getFunctionId(p) == mvcRef) return FALSE;
		if (getFunctionId(p) == singleRef) return FALSE;
		/* the update instructions for SQL has side effects.
		   whether this is relevant should be explicitly checked
		   in the environment of the call */
		if (isUpdateInstruction(p)) return FALSE;
		return TRUE;
	}
	if( getModuleId(p) == languageRef){
		if( getFunctionId(p) == assertRef) return TRUE;
		return FALSE;
	}
	if (getModuleId(p) == constraintsRef)
		return FALSE;
	if( getModuleId(p) == mapiRef){
		if( getFunctionId(p) == rpcRef)
			return TRUE;
		if( getFunctionId(p) == reconnectRef)
			return TRUE;
		if( getFunctionId(p) == disconnectRef)
			return TRUE;
	}
	if (strict &&  getFunctionId(p) == newRef &&
		getModuleId(p) != groupRef )
		return TRUE;

	if ( getModuleId(p) == octopusRef){
		if (getFunctionId(p) == bindRef) return FALSE;
		if (getFunctionId(p) == bindidxRef) return FALSE;
		if (getFunctionId(p) == binddbatRef) return FALSE;
		return TRUE;
	}
	if ( getModuleId(p) == remoteRef)
		return TRUE;
	return FALSE;
}
/*
 * @-
 * Side-effect free functions are crucial for several operators.
 */
int
isSideEffectFree(MalBlkPtr mb){
	int i;
	for(i=1; i< mb->stop && getInstrPtr(mb,i)->token != ENDsymbol; i++){
		if( hasSideEffects(getInstrPtr(mb,i), TRUE))
			return FALSE;
	}
	return TRUE;
}
/*
 * @-
 * Breaking up a MAL program into pieces for distributed requires
 * identification of (partial) blocking instructions. A conservative
 * definition can be used.
 */
int
isBlocking(InstrPtr p)
{
	if (blockStart(p) || blockExit(p) || blockCntrl(p))
		return TRUE;

	if ( getFunctionId(p) == sortTailRef ||
		 getFunctionId(p) == sortHRef ||
		 getFunctionId(p) == sortHTRef ||
		 getFunctionId(p) == sortTHRef )
		return TRUE;

	if( getModuleId(p) == aggrRef ||
		getModuleId(p) == groupRef ||
		getModuleId(p) == sqlRef )
			return TRUE;
	return FALSE;
}

int isAllScalar(MalBlkPtr mb, InstrPtr p)
{
	int i;
	for (i=p->retc; i<p->argc; i++)
	if (isaBatType(getArgType(mb,p,i)) || getArgType(mb,p,i)==TYPE_bat)
		return FALSE;
	return TRUE;
}

/*
 * @-
 * Used in the merge table optimizer. It is built incrementally
 * and should be conservative.
 */
int isMapOp(InstrPtr p){
	return	(getModuleId(p) == multiplexRef) ||
		(getModuleId(p)== batcalcRef && getFunctionId(p) != mark_grpRef && getFunctionId(p) != rank_grpRef) ||
		(getModuleId(p)== batmtimeRef) ||
		(getModuleId(p)== batstrRef) ||
		(getModuleId(p)== mkeyRef);
}

int isTopn(InstrPtr p){
	return ((getModuleId(p) == pqueueRef &&
		(getFunctionId(p) == topn_minRef ||
		 getFunctionId(p) == topn_maxRef ||
		 getFunctionId(p) == utopn_minRef ||
		 getFunctionId(p) == utopn_maxRef)) || isSlice(p));
}

int isSlice(InstrPtr p){
	return (getModuleId(p) == algebraRef &&
		getFunctionId(p) == sliceRef);
}

int isOrderby(InstrPtr p){
	return ((getModuleId(p) == algebraRef &&
		(getFunctionId(p) == sortTailRef ||
		 getFunctionId(p) == sortReverseTailRef)) ||
		(getModuleId(p) == groupRef &&
		(getFunctionId(p) == refineRef ||
		 getFunctionId(p) == refine_reverseRef)));
}

int isDiffOp(InstrPtr p){
	return (getModuleId(p) == algebraRef &&
	    	(getFunctionId(p) == semijoinRef ||
 	     	 getFunctionId(p) == kdifferenceRef));
}

int isProjection(InstrPtr p){
	return (getModuleId(p) == algebraRef &&
                 getFunctionId(p) == leftjoinRef
		);
}

int isMatJoinOp(InstrPtr p){
	return (getModuleId(p) == algebraRef &&
                (getFunctionId(p) == joinRef ||
/*               getFunctionId(p) == antijoinRef || is not mat save */
                 getFunctionId(p) == leftjoinRef ||
                 getFunctionId(p) == thetajoinRef ||
                 getFunctionId(p) == bandjoinRef)
		);
}

int isFragmentGroup(InstrPtr p){
	return
/*
			(getModuleId(p)== constraintsRef &&
				getFunctionId(p)== putName("emptySet",8)) ||
*/
			(getModuleId(p)== pcreRef && (
			getFunctionId(p)== likeselectRef ||
			getFunctionId(p)== likeuselectRef  ||
			getFunctionId(p)== ilikeselectRef  ||
			getFunctionId(p)== ilikeuselectRef
			))  ||
			(getModuleId(p)== algebraRef && (
				getFunctionId(p)== selectRef ||
				getFunctionId(p)== selectNotNilRef ||
				getFunctionId(p)== uselectRef ||
				getFunctionId(p)== projectRef ||
				getFunctionId(p)== antiuselectRef ||
				getFunctionId(p)== thetauselectRef ||
				getFunctionId(p)== reuseRef
			)	)  ||
			(getModuleId(p)== pcreRef && (
				getFunctionId(p)== likeselectRef ||
				getFunctionId(p)== likeuselectRef
			)	)  ||
			(getModuleId(p)== batRef && (
				getFunctionId(p)== reverseRef ||
				getFunctionId(p)== mirrorRef /*||
				getFunctionId(p)== setAccessRef ||
				getFunctionId(p)== setWriteModeRef */
			) );
}

int isSelect(InstrPtr p)
{
	return  (getModuleId(p)== pcreRef && (
			getFunctionId(p)== likeselectRef ||
			getFunctionId(p)== likeuselectRef  ||
			getFunctionId(p)== ilikeselectRef  ||
			getFunctionId(p)== ilikeuselectRef
			))  ||
			(getModuleId(p)== algebraRef && (
		getFunctionId(p)== selectRef ||
		getFunctionId(p)== selectNotNilRef ||
		getFunctionId(p)== uselectRef ||
		getFunctionId(p)== projectRef ||
		getFunctionId(p)== antiuselectRef ||
		getFunctionId(p)== thetauselectRef ||
		getFunctionId(p)== likeselectRef ||
		getFunctionId(p)== likeuselectRef ));
}
/*
 * @-
 * Some optimizers are interdependent (e.g. mitosis and octopus), which
 * requires inspection of the pipeline attached to a MAL block.
 */
int
isOptimizerEnabled(MalBlkPtr mb, str opt)
{
	int i;
	InstrPtr q;

	for (i= mb->stop-1; i > 0; i--){
		q= getInstrPtr(mb,i);
		if ( q->token == ENDsymbol)
			break;
		if ( getModuleId(q) == optimizerRef &&
			 getFunctionId(q) == opt)
			return 1;
	}
	return 0;
}
wrd
getVarRows(MalBlkPtr mb, int v)
{
	VarPtr p = varGetProp(mb, v, rowsProp);

	if (!p)
		return -1;
	if (p->value.vtype == TYPE_wrd
#if SIZEOF_BUN <= SIZEOF_WRD
		    && p->value.val.wval <= (wrd) BUN_MAX
#endif
		)
		return p->value.val.wval;
	if (p->value.vtype == TYPE_lng
#if SIZEOF_BUN <= SIZEOF_LNG
		    && p->value.val.lval <= (lng) BUN_MAX
#endif
		)
		return (wrd)p->value.val.lval;
	if (p->value.vtype == TYPE_int
#if SIZEOF_BUN <= SIZEOF_INT
		    && p->value.val.ival <= (int) BUN_MAX
#endif
		)
		return p->value.val.ival;
	if (p->value.vtype == TYPE_sht)
		return p->value.val.shval;
	if (p->value.vtype == TYPE_bte)
		return p->value.val.btval;
	return -1;
}

