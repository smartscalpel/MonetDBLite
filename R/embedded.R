monetdb_embedded_env <- new.env(parent=emptyenv())
monetdb_embedded_env$is_started <- FALSE
monetdb_embedded_env$started_dir <- ""

classname <- "monetdb_embedded_connection"

monetdb_embedded_startup <- function(dir=tempdir(), quiet=TRUE, sequential=TRUE) {
	quiet <- as.logical(quiet)
	dir <- as.character(dir)
	if (length(dir) != 1) {
		stop("Need a single directory name as parameter.")
	}
	if (!dir.exists(dir) && !dir.create(dir, recursive=T)) {
		stop("Cannot create ", dir)
	}
	if (file.access(dir, mode=2) < 0) {
		stop("Cannot write to ", dir)
	}
	dir <- normalizePath(dir, mustWork=T)
	if (!monetdb_embedded_env$is_started) {
		res <- .Call(monetdb_startup_R, dir, quiet, 
			getOption('monetdb.squential', sequential))
	} else {
		if (dir != monetdb_embedded_env$started_dir) {
			stop("MonetDBLite cannot change database directories\n(already started in ", monetdb_embedded_env$started_dir, ",\nshutdown with `DBI::dbDisconnect(con, shutdown=TRUE)` or `MonetDBLite::monetdblite_shutdown()` first).")
		}
		return(invisible(TRUE))
	}
	if (is.character(res)) {
		stop("Failed to initialize embedded MonetDB ", gsub("\n", " ", res, fixed=TRUE))
	}
	monetdb_embedded_env$is_started <- TRUE
	monetdb_embedded_env$started_dir <- dir
	invisible(TRUE)
}

monetdb_embedded_query <- function(conn, query, execute=TRUE, resultconvert=TRUE) {
	if (!inherits(conn, classname)) {
		stop("Invalid connection")
	}
	query <- as.character(query)
	if (length(query) != 1) {
		stop("Need a single query as parameter.")
	}
	if (!monetdb_embedded_env$is_started) {
		stop("Call monetdb_embedded_startup() first")
	}
	if (!dir.exists(file.path(monetdb_embedded_env$started_dir, "bat"))) {
		stop("Someone killed all the BATs! Call Brigitte Bardot!")
	}
	execute <- as.logical(execute)
	if (length(execute) != 1) {
		stop("Need a single execute flag as parameter.")
	}
	resultconvert <- as.logical(resultconvert)
	if (length(resultconvert) != 1) {
		stop("Need a single resultconvert flag as parameter.")
	}
	# make sure the query is terminated
	query <- paste(query, "\n;", sep="")
	res <- .Call(monetdb_query_R, conn, query, execute, resultconvert, interactive() && getOption("monetdb.progress", FALSE))

	resp <- list()
	if (is.character(res)) { # error
		resp$type <- "!" # MSG_MESSAGE
		resp$message <- gsub("\n", " ", res, fixed=TRUE)
	}
	if (is.numeric(res)) { # no result set, but successful
		resp$type <- 2 # Q_UPDATE
		resp$rows <- res
	}
	if (is.list(res)) {
		resp$type <- 1 # Q_TABLE
		attr(res, "row.names") <- c(NA_integer_, as.integer(-1 * attr(res, "__rows")))
  		class(res) <- "data.frame"
		names(res) <- gsub("\\", "", names(res), fixed=T)
		resp$tuples <- res
	}
	resp
}

monetdb_embedded_append <- function(conn, table, tdata, schema="sys") {
	table <- as.character(table)
	table <- gsub("(^\"|\"$)", "", table)
	if (!inherits(conn, classname)) {
		stop("Invalid connection")
	}
	if (!monetdb_embedded_env$is_started) {
		stop("Call monetdb_embedded_startup() first")
	}
	if (!dir.exists(file.path(monetdb_embedded_env$started_dir, "bat"))) {
		stop("Someone killed all the BATs! Call Brigitte Bardot!")
	}
	if (length(table) != 1) {
		stop("Need a single table name as parameter.")
	}
	schema <- as.character(schema)
	if (length(schema) != 1) {
		stop("Need a single schema name as parameter.")
	}
	if (!is.data.frame(tdata)) {
		stop("Need a data frame as tdata parameter.")
	}
	
	.Call(monetdb_append_R, conn, schema, table, tdata)

}


monetdb_embedded_connect <- function() {
	if (!monetdb_embedded_env$is_started) {
		stop("Call monetdb_embedded_startup() first")
	}
	conn <- .Call(monetdb_connect_R)

	class(conn) <- classname
	return(conn)
}

monetdb_embedded_disconnect <- function(conn) {
	if (!inherits(conn, classname)) {
		stop("Invalid connection")
	}
	.Call(monetdb_disconnect_R, conn)

	invisible(TRUE)
}

monetdb_embedded_shutdown <- monetdblite_shutdown <- function() {
    gc()	
        
	if (monetdb_embedded_env$started_dir != "" && !dir.exists(monetdb_embedded_env$started_dir)) {
		stop("Somehow the database directory went missing ", monetdb_embedded_env$started_dir)
	}
	.Call(monetdb_shutdown_R)

	monetdb_embedded_env$is_started <- FALSE
	monetdb_embedded_env$started_dir <- ""
	invisible(TRUE)
}
