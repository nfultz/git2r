## git2r, R bindings to the libgit2 library.
## Copyright (C) 2013-2018 The git2r contributors
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

##' Create tag targeting HEAD commit in repository
##'
##' @param object The repository \code{object}.
##' @param name Name for the tag.
##' @param message The tag message.
##' @param session Add sessionInfo to tag message. Default is FALSE.
##' @param tagger The tagger (author) of the tag
##' @return invisible(\code{git_tag}) object
##' @export
##' @examples
##' \dontrun{
##' ## Initialize a temporary repository
##' path <- tempfile(pattern="git2r-")
##' dir.create(path)
##' repo <- init(path)
##'
##' ## Create a user
##' config(repo, user.name="Alice", user.email="alice@@example.org")
##'
##' ## Commit a text file
##' writeLines("Hello world!", file.path(path, "example.txt"))
##' add(repo, "example.txt")
##' commit(repo, "First commit message")
##'
##' ## Create tag
##' tag(repo, "Tagname", "Tag message")
##'
##' ## List tags
##' tags(repo)
##' }
tag <- function(object = ".",
                name    = NULL,
                message = NULL,
                session = FALSE,
                tagger  = NULL)
{
    object <- lookup_repository(object)

    stopifnot(is.character(message),
              identical(length(message), 1L),
              nchar(message[1]) > 0)
    if (isTRUE(session))
        message <- add_session_info(message)

    if (is.null(tagger))
        tagger <- default_signature(object)

    invisible(.Call(git2r_tag_create, object, name, message, tagger))
}

##' Delete an existing tag reference
##'
##' @param object Can be either the path (default is ".") to a
##'     repository, or a \code{\linkS4class{git_repository}} object,
##'     or a \code{\linkS4class{git_tag}} object. or the tag name.
##' @param name If the \code{object} argument is a path to a
##'     repository or a \code{git_repository}, the name of the tag to
##'     delete.
##' @return \code{invisible(NULL)}
##' @export
##' @examples
##' \dontrun{
##' ## Initialize a temporary repository
##' path <- tempfile(pattern="git2r-")
##' dir.create(path)
##' repo <- init(path)
##'
##' ## Create a user
##' config(repo, user.name="Alice", user.email="alice@@example.org")
##'
##' ## Commit a text file
##' writeLines("Hello world!", file.path(path, "example.txt"))
##' add(repo, "example.txt")
##' commit(repo, "First commit message")
##'
##' ## Create two tags
##' tag(repo, "Tag1", "Tag message 1")
##' t2 <- tag(repo, "Tag2", "Tag message 2")
##'
##' ## List the two tags in the repository
##' tags(repo)
##'
##' ## Delete the two tags in the repository
##' tag_delete(repo, "Tag1")
##' tag_delete(t2)
##'
##' ## Show the empty list with tags in the repository
##' tags(repo)
##' }
tag_delete <- function(object = ".", name = NULL) {
    if (is_tag(object)) {
        name <- object@name
        object <- object@repo
    } else {
        object <- lookup_repository(object)
    }

    .Call(git2r_tag_delete, object, name)
    invisible(NULL)
}

##' Tags
##'
##' @template repo-param
##' @return list of tags in repository
##' @export
##' @examples
##' \dontrun{
##' ## Initialize a temporary repository
##' path <- tempfile(pattern="git2r-")
##' dir.create(path)
##' repo <- init(path)
##'
##' ## Create a user
##' config(repo, user.name="Alice", user.email="alice@@example.org")
##'
##' ## Commit a text file
##' writeLines("Hello world!", file.path(path, "example.txt"))
##' add(repo, "example.txt")
##' commit(repo, "First commit message")
##'
##' ## Create tag
##' tag(repo, "Tagname", "Tag message")
##'
##' ## List tags
##' tags(repo)
##' }
tags <- function(repo = ".") {
    .Call(git2r_tag_list, lookup_repository(repo))
}

##' Brief summary of a tag
##'
##' @aliases show,git_tag-methods
##' @docType methods
##' @param object The tag \code{object}
##' @return None (invisible 'NULL').
##' @keywords methods
##' @export
##' @examples
##' \dontrun{
##' ## Initialize a temporary repository
##' path <- tempfile(pattern="git2r-")
##' dir.create(path)
##' repo <- init(path)
##'
##' ## Create a user
##' config(repo, user.name="Alice", user.email="alice@@example.org")
##'
##' ## Commit a text file
##' writeLines("Hello world!", file.path(path, "example.txt"))
##' add(repo, "example.txt")
##' commit(repo, "First commit message")
##'
##' ## Create tag
##' tag(repo, "Tagname", "Tag message")
##'
##' ## View brief summary of tag
##' tags(repo)[[1]]
##' }
setMethod("show",
          signature(object = "git_tag"),
          function(object)
          {
              cat(sprintf("[%s] %s\n",
                          substr(object@target, 1 , 6),
                          object@name))
          }
)

##' Summary of a tag
##'
##' @aliases summary,git_tag-methods
##' @docType methods
##' @param object The tag \code{object}
##' @param ... Additional arguments affecting the summary produced.
##' @return None (invisible 'NULL').
##' @keywords methods
##' @export
##' @examples
##' \dontrun{
##' ## Initialize a temporary repository
##' path <- tempfile(pattern="git2r-")
##' dir.create(path)
##' repo <- init(path)
##'
##' ## Create a user
##' config(repo, user.name="Alice", user.email="alice@@example.org")
##'
##' ## Commit a text file
##' writeLines("Hello world!", file.path(path, "example.txt"))
##' add(repo, "example.txt")
##' commit(repo, "First commit message")
##'
##' ## Create tag
##' tag(repo, "Tagname", "Tag message")
##'
##' ## Summary of tag
##' summary(tags(repo)[[1]])
##' }
setMethod("summary",
          signature(object = "git_tag"),
          function(object, ...)
          {
              cat(sprintf(paste0("name:    %s\n",
                                 "target:  %s\n",
                                 "tagger:  %s <%s>\n",
                                 "when:    %s\n",
                                 "message: %s\n"),
                          object@name,
                          object@target,
                          object@tagger@name,
                          object@tagger@email,
                          as(object@tagger@when, "character"),
                          object@message))
          }
)
