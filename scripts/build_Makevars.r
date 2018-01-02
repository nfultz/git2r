## git2r, R bindings to the libgit2 library.
## Copyright (C) 2013-2017 The git2r contributors
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License, version 2,
## as published by the Free Software Foundation.
##
## git2r is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

##' Generate object files in path
##'
##' @param path The path to directory to generate object files from
##' @param exclude Files to exclude
##' @return Character vector with object files
o_files <- function(path, exclude = NULL) {
    files <- sub("c$", "o",
                 sub("src/", "",
                     list.files(path, pattern = "[.]c$", full.names = TRUE)))

    if (!is.null(exclude))
        files <- files[!(files %in% exclude)]
    files
}

##' Generate build objects
##'
##' @param files The object files
##' @param substitution Any substitutions to apply in OBJECTS
##' @param Makevars The Makevars file
##' @return invisible NULL
build_objects <- function(files, substitution, Makevars) {
    lapply(names(files), function(obj) {
        cat("OBJECTS.", obj, " =", sep="", file = Makevars)
        len <- length(files[[obj]])
        for (i in seq_len(len)) {
            prefix <- ifelse(all(i > 1, (i %% 3) == 1), "    ", " ")
            postfix <- ifelse(all(i > 1, i < len, (i %% 3) == 0), " \\\n", "")
            cat(prefix, files[[obj]][i], postfix, sep="", file = Makevars)
        }
        cat("\n\n", file = Makevars)
    })

    cat("LIBGIT =", file = Makevars)
    len <- length(names(files))
    for (i in seq_len(len)) {
        prefix <- ifelse(all(i > 1, (i %% 3) == 1), "    ", " ")
        postfix <- ifelse(all(i > 1, i < len, (i %% 3) == 0), " \\\n", "")
        cat(prefix, "$(OBJECTS.", names(files)[i], ")", postfix, sep="", file = Makevars)
    }

    if (!is.null(substitution))
        cat(substitution, file = Makevars)
    cat("\n", file = Makevars)

    invisible(NULL)
}

##' Build Makevars.in
##'
##' @return invisible NULL
build_Makevars.in <- function() {
    Makevars <- file("src/Makevars.in", "w")
    on.exit(close(Makevars))

    files <- list(libgit2 = o_files("src/libgit2/src"),
                  ## FIXME: Uncomment when updating to libgit 0.26 + 1.
                  ## libgit2.streams = o_files("src/libgit2/src/streams"),
                  libgit2.transports =
                      o_files("src/libgit2/src/transports",
                              c("libgit2/src/transports/auth_negotiate.o",
                                "libgit2/src/transports/winhttp.o")),
                  libgit2.unix = o_files("src/libgit2/src/unix"),
                  libgit2.xdiff = o_files("src/libgit2/src/xdiff"),
                  http_parser = o_files("src/libgit2/deps/http-parser"))

    cat("# Generated by scripts/build_Makevars.r: do not edit by hand\n\n",
        file=Makevars)
    cat("PKG_CFLAGS = @PKG_CFLAGS@\n", file = Makevars)
    cat("PKG_CPPFLAGS = @PKG_CPPFLAGS@\n", file = Makevars)
    cat("PKG_LIBS = -L. -lmygit @PKG_LIBS@\n", file = Makevars)
    cat("\n", file = Makevars)

    build_objects(files, " @GIT2R_SRC_REGEX@", Makevars)

    cat("\n$(SHLIB): libmygit.a\n\n", file = Makevars)
    cat("libmygit.a: $(LIBGIT)\n\t$(AR) rcs libmygit.a $(LIBGIT)\n\n", file = Makevars)
    cat("clean:\n\trm -f *.o libmygit.a git2r.so $(LIBGIT)\n\n", file = Makevars)
    cat(".PHONY: all clean\n", file = Makevars)

    invisible(NULL)
}

##' Extract .NAME in .Call(.NAME
##'
##' @param files R files to extract .NAME from
##' @return data.frame with columns filename and .NAME
extract_git2r_calls <- function(files) {
    df <- lapply(files, function(filename) {
        ## Read file
        lines <- readLines(file.path("R", filename))

        ## Trim comments
        comments <- gregexpr("#", lines)
        for (i in seq_len(length(comments))) {
            start <- as.integer(comments[[i]])
            if (start[1] > 0) {
                if (start[1] > 1) {
                    lines[i] <- substr(lines[i], 1, start[1])
                } else {
                    lines[i] <- ""
                }
            }
        }

        ## Trim whitespace
        lines <- sub("^\\s*", "", sub("\\s*$", "", lines))

        ## Collapse to one line
        lines <- paste0(lines, collapse=" ")

        ## Find .Call
        pattern <- "[.]Call[[:space:]]*[(][[:space:]]*[.[:alpha:]\"][^\\),]*"
        calls <- gregexpr(pattern, lines)
        start <- as.integer(calls[[1]])

        if (start[1] > 0) {
            ## Extract .Call
            len <- attr(calls[[1]], "match.length")
            calls <- substr(rep(lines, length(start)), start, start + len - 1)

            ## Trim .Call to extract .NAME
            pattern <- "[.]Call[[:space:]]*[(][[:space:]]*"
            calls <- sub(pattern, "", calls)
            return(data.frame(filename = filename,
                              .NAME = calls,
                              stringsAsFactors = FALSE))
        }

        return(NULL)
    })

    df <- do.call("rbind", df)
    df[order(df$filename),]
}

##' Check that .NAME in .Call(.NAME is prefixed with 'git2r_'
##'
##' Raise an error in case of missing 'git2r_' prefix
##' @param calls data.frame with the name of the C function to call
##' @return invisible NULL
check_git2r_prefix <- function(calls) {
    .NAME <- grep("git2r_", calls$.NAME, value=TRUE, invert=TRUE)

    if (!identical(length(.NAME), 0L)) {
        i <- which(calls$.NAME == .NAME)
        msg <- sprintf("%s in %s\n", calls$.NAME[i], calls$filename[i])
        msg <- c("\n\nMissing 'git2r_' prefix:\n", msg, "\n")
        stop(msg)
    }

    invisible(NULL)
}

##' Check that .NAME is a registered symbol in .Call(.NAME
##'
##' Raise an error in case of .NAME is of the form "git2r_"
##' @param calls data.frame with the name of the C function to call
##' @return invisible NULL
check_git2r_use_registered_symbol <- function(calls) {
    .NAME <- grep("^\"", calls$.NAME)

    if (!identical(length(.NAME), 0L)) {
        msg <- sprintf("%s in %s\n", calls$.NAME[.NAME], calls$filename[.NAME])
        msg <- c("\n\nUse registered symbol instead of:\n", msg, "\n")
        stop(msg)
    }

    invisible(NULL)
}

## Check that all git2r C functions are prefixed with 'git2r_' and
## registered
calls <- extract_git2r_calls(list.files("R", "*.r"))
check_git2r_prefix(calls)
check_git2r_use_registered_symbol(calls)

## Generate Makevars
build_Makevars.in()
