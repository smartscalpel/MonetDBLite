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
 * Copyright August 2008-2014 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * author M. Kersten
 *
 * MAL builder
 * The MAL builder library containst the primitives to simplify construction
 * of programs by compilers. It has grown out of the MonetDB/SQL code generator.
 * The strings being passed as arguments are copied in the process.
 *
 */
#include "monetdb_config.h"
#include "mal_builder.h"
InstrPtr
newAssignment(MalBlkPtr mb)
{
	InstrPtr q = newInstruction(mb,ASSIGNsymbol);

	if (q == NULL)
		return NULL;
	if ((getArg(q,0)= newTmpVariable(mb,TYPE_any)) < 0) {
		freeInstruction(q);
		return NULL;
	}
	pushInstruction(mb, q);
	if (mb->errors) {
		freeInstruction(q);
		return NULL;
	}
	return q;
}

InstrPtr
newStmt(MalBlkPtr mb, char *module, char *name)
{
	InstrPtr q = newInstruction(mb,ASSIGNsymbol);

	if (q == NULL)
		return NULL;
	setModuleId(q, putName(module, strlen(module)));
	setFunctionId(q, putName(name, strlen(name)));
	setDestVar(q, newTmpVariable(mb, TYPE_any));
	if (getDestVar(q) < 0) {
		freeInstruction(q);
		return NULL;
	}
	pushInstruction(mb, q);
	if (mb->errors) {
		freeInstruction(q);
		return NULL;
	}
	return q;
}

InstrPtr
newStmt1(MalBlkPtr mb, str module, char *name)
{
	InstrPtr q = newInstruction(mb,ASSIGNsymbol);

	if (q == NULL)
		return NULL;
	setModuleId(q, putName(module, strlen(module)));
	setFunctionId(q, putName(name, strlen(name)));
	setDestVar(q, newTmpVariable(mb, TYPE_any));
	if (getDestVar(q) < 0) {
		freeInstruction(q);
		return NULL;
	}
	pushInstruction(mb, q);
	if (mb->errors) {
		freeInstruction(q);
		return NULL;
	}
	return q;
}

InstrPtr
newStmt2(MalBlkPtr mb, str module, char *name)
{
	InstrPtr q = newInstruction(mb,ASSIGNsymbol);

	if (q == NULL)
		return NULL;
	setModuleId(q, putName(module, strlen(module)));
	setFunctionId(q, putName(name, strlen(name)));
	setDestVar(q, newTmpVariable(mb, TYPE_any));
	if (getDestVar(q) < 0) {
		freeInstruction(q);
		return NULL;
	}
	pushInstruction(mb, q);
	if (mb->errors) {
		freeInstruction(q);
		return NULL;
	}
	return q;
}

InstrPtr
newReturnStmt(MalBlkPtr mb)
{
	InstrPtr q = newInstruction(mb,ASSIGNsymbol);

	if (q == NULL)
		return NULL;
	if ((getArg(q,0)= newTmpVariable(mb,TYPE_any)) < 0) {
		freeInstruction(q);
		return NULL;
	}
	pushInstruction(mb, q);
	if (mb->errors) {
		freeInstruction(q);
		return NULL;
	}
	q->barrier= RETURNsymbol;
	return q;
}

InstrPtr
newFcnCall(MalBlkPtr mb, char *mod, char *fcn)
{
	InstrPtr q = newAssignment(mb);

	if (q == NULL)
		return NULL;
	setModuleId(q, putName(mod, strlen(mod)));
	setFunctionId(q, putName(fcn, strlen(fcn)));
	return q;
}

InstrPtr
newComment(MalBlkPtr mb, const char *val)
{
	InstrPtr q = newInstruction(NULL,REMsymbol);
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_str;
	if ((cst.val.sval= GDKstrdup(val)) == NULL) {
		freeInstruction(q);
		return NULL;
	}
	cst.len= (int) strlen(cst.val.sval);
	getArg(q,0) = defConstant(mb,TYPE_str,&cst);
	clrVarConstant(mb,getArg(q,0));
	setVarDisabled(mb,getArg(q,0));
	pushInstruction(mb, q);
	if (mb->errors) {
		freeInstruction(q);
		return NULL;
	}
	return q;
}

InstrPtr
newCatchStmt(MalBlkPtr mb, str nme)
{
	InstrPtr q = newAssignment(mb);
	int i= findVariable(mb,nme);

	if (q == NULL)
		return NULL;
	q->barrier = CATCHsymbol;
	if ( i< 0) {
		if ((getArg(q,0)= newVariable(mb, GDKstrdup(nme),TYPE_str)) < 0) {
			freeInstruction(q);
			return NULL;
		}
		setVarUDFtype(mb,getArg(q,0));
	} else getArg(q,0) = i;
	return q;
}
InstrPtr
newRaiseStmt(MalBlkPtr mb, str nme)
{
	InstrPtr q = newAssignment(mb);
	int i= findVariable(mb,nme);

	if (q == NULL)
		return NULL;
	q->barrier = RAISEsymbol;
	if ( i< 0) {
		if ((getArg(q,0)= newVariable(mb, GDKstrdup(nme),TYPE_str)) < 0) {
			freeInstruction(q);
			return NULL;
		}
	} else
		getArg(q,0) = i;
	return q;
}

InstrPtr
newExitStmt(MalBlkPtr mb, str nme)
{
	InstrPtr q = newAssignment(mb);
	int i= findVariable(mb,nme);

	if (q == NULL)
		return NULL;
	q->barrier = EXITsymbol;
	if ( i< 0) {
		if ((getArg(q,0)= newVariable(mb, GDKstrdup(nme),TYPE_str)) < 0) {
			freeInstruction(q);
			return NULL;
		}
	} else
		getArg(q,0) = i;
	return q;
}

InstrPtr
pushInt(MalBlkPtr mb, InstrPtr q, int val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_int;
	cst.val.ival= val;
	cst.len = 0;
	_t = defConstant(mb, TYPE_int,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushWrd(MalBlkPtr mb, InstrPtr q, wrd val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_wrd;
	cst.val.wval= val;
	cst.len = 0;
	_t = defConstant(mb, TYPE_wrd,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushBte(MalBlkPtr mb, InstrPtr q, bte val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_bte;
	cst.val.btval= val;
	cst.len = 0;
	_t = defConstant(mb, TYPE_bte,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushOid(MalBlkPtr mb, InstrPtr q, oid val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_oid;
	cst.val.oval= val;
	cst.len = 0;
	_t = defConstant(mb,TYPE_oid,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushVoid(MalBlkPtr mb, InstrPtr q)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_void;
	cst.val.oval= oid_nil;
	cst.len = 0;
	_t = defConstant(mb,TYPE_void,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushLng(MalBlkPtr mb, InstrPtr q, lng val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_lng;
	cst.val.lval= val;
	cst.len = 0;
	_t = defConstant(mb,TYPE_lng,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushDbl(MalBlkPtr mb, InstrPtr q, dbl val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_dbl;
	cst.val.dval= val;
	cst.len = 0;
	_t = defConstant(mb,TYPE_dbl,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushFlt(MalBlkPtr mb, InstrPtr q, flt val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_flt;
	cst.val.fval= val;
	cst.len = 0;
	_t = defConstant(mb,TYPE_flt,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushStr(MalBlkPtr mb, InstrPtr q, const char *Val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_str;
	if ((cst.val.sval= GDKstrdup(Val)) == NULL) {
		freeInstruction(q);
		return NULL;
	}
	cst.len= (int) strlen(cst.val.sval);
	_t = defConstant(mb,TYPE_str,&cst);
	return pushArgument(mb, q, _t);
}

InstrPtr
pushBit(MalBlkPtr mb, InstrPtr q, bit val)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype= TYPE_bit;
	cst.val.btval= val;
	cst.len = 0;
	_t = defConstant(mb,TYPE_bit,&cst);

	return pushArgument(mb, q, _t);
}

InstrPtr
pushNil(MalBlkPtr mb, InstrPtr q, int tpe)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.len = 0;
	if( !isaBatType(tpe) && tpe != TYPE_bat ) {
		assert(tpe < MAXATOMS);	/* in particular, tpe!=TYPE_any */
		if (!tpe) {
			cst.vtype=TYPE_void;
			cst.val.oval= oid_nil;
		} else if (ATOMextern(tpe)) {
			ptr p = ATOMnil(tpe);
			VALset(&cst, tpe, p);
		} else {
			ptr p = ATOMnilptr(tpe);
			VALset(&cst, tpe, p);
		}
		_t = defConstant(mb,tpe,&cst);
	} else {
		cst.vtype = TYPE_bat;
		cst.val.bval = 0;
		_t = defConstant(mb,TYPE_bat,&cst);
		mb->var[_t]->type = tpe;
	}
	q= pushArgument(mb, q, _t);
	setVarUDFtype(mb,getArg(q,q->argc-1)); /* needed */
	return q;
}

InstrPtr
pushNilType(MalBlkPtr mb, InstrPtr q, char *tpe)
{
	int _t,idx;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	idx= getTypeIndex(tpe, -1, TYPE_any);
	if( idx < 0 || idx >= GDKatomcnt || idx >= MAXATOMS)
		return NULL;
	cst.vtype=TYPE_void;
	cst.val.oval= oid_nil;
	cst.len = 0;
	convertConstant(idx, &cst);
	_t = defConstant(mb,idx,&cst);
	setVarUDFtype(mb,_t);

	return pushArgument(mb, q, _t);
}
InstrPtr
pushType(MalBlkPtr mb, InstrPtr q, int tpe)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype=TYPE_void;
	cst.val.oval= oid_nil;
	cst.len = 0;
	convertConstant(tpe, &cst);
	_t = defConstant(mb,tpe,&cst);
	setVarUDFtype(mb,_t);

	return pushArgument(mb, q, _t);
}

InstrPtr
pushZero(MalBlkPtr mb, InstrPtr q, int tpe)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	cst.vtype=TYPE_int;
	cst.val.ival= 0;
	cst.len = 0;
	convertConstant(tpe, &cst);
	_t = defConstant(mb,tpe,&cst);

	return pushArgument(mb, q, _t);
}

InstrPtr
pushEmptyBAT(MalBlkPtr mb, InstrPtr q, int tpe)
{
	if (q == NULL)
		return NULL;
	getModuleId(q) = getName("bat",3);
	getFunctionId(q) = getName("new",3);

	q = pushArgument(mb, q, newTypeVariable(mb,getHeadType(tpe)));
	q = pushArgument(mb, q, newTypeVariable(mb,getTailType(tpe)));
	q = pushZero(mb,q,TYPE_lng);
	return q;
}

InstrPtr
pushValue(MalBlkPtr mb, InstrPtr q, ValPtr vr)
{
	int _t;
	ValRecord cst;

	if (q == NULL)
		return NULL;
	VALcopy(&cst, vr);
	_t = defConstant(mb,cst.vtype,&cst);
	return pushArgument(mb, q, _t);
}
