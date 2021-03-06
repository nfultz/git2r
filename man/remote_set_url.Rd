% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/remote.R
\name{remote_set_url}
\alias{remote_set_url}
\title{Set the remote's url in the configuration}
\usage{
remote_set_url(repo = ".", name = NULL, url = NULL)
}
\arguments{
\item{repo}{a path to a repository or a
\code{\linkS4class{git_repository}} object. Default is '.'}

\item{name}{The name of the remote}

\item{url}{The \code{url} to set}
}
\value{
NULL, invisibly
}
\description{
This assumes the common case of a single-url remote and will
otherwise raise an error.
}
\examples{
\dontrun{
## Initialize a temporary repository
path <- tempfile(pattern="git2r-")
dir.create(path)
repo <- init(path)

## Create a user and commit a file
config(repo, user.name="Alice", user.email="alice@example.org")
writeLines("Hello world!", file.path(path, "example.txt"))
add(repo, "example.txt")
commit(repo, "First commit message")

## Add a remote
remote_add(repo, "playground", "https://example.org/git2r/playground")
remotes(repo)
remote_url(repo, "playground")

## Rename a remote
remote_rename(repo, "playground", "foobar")
remotes(repo)
remote_url(repo, "foobar")

## Set remote url
remote_set_url(repo, "foobar", "https://example.org/git2r/foobar")
remotes(repo)
remote_url(repo, "foobar")

## Remove a remote
remote_remove(repo, "foobar")
remotes(repo)
}
}
