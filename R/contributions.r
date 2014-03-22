## git2r, R bindings to the libgit2 library.
## Copyright (C) 2013-2014  Stefan Widgren
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, version 2 of the License.
##
## git2r is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

##' Contributions
##'
##' See contributions to a Git repo
##' @name contributions-methods
##' @aliases contributions
##' @aliases contributions-methods
##' @aliases contributions,git_repository-method
##' @aliases contributions,character-method
##' @docType methods
##' @param repo The repository.
##' @param breaks Default is \code{month}. Change to week or day as necessary.
##' @param by Contributions by 'commits' or 'user'. Default is 'commits'.
##' @return A \code{data.frame} with contributions.
##' @importFrom reshape2 melt dcast
##' @keywords methods
##' @export
##' @author Karthik Ram \email{karthik.ram@@gmail.com}
##' @author Stefan Widgren \email{stefan.widgren@@gmail.com}
##' @examples \dontrun{
##' ## If current working dir is a git repo
##' contributions()
##' contributions(breaks = "week")
##' contributions(breaks = "day")
##'
##' ## If the path is somewhere else
##' contributions("/path/to/repo")
##'}
setGeneric('contributions',
           signature = 'repo',
           function(repo,
                    breaks = c('month', 'week', 'day'),
                    by = c('commits', 'user'))
           standardGeneric('contributions'))

setMethod('contributions',
          signature(repo = 'missing'),
          function (repo, breaks, by)
          {
              ## Try current working directory
              contributions(getwd(), breaks = breaks, by = by)
          }
)

setMethod('contributions',
          signature(repo = 'character'),
          function (repo, breaks, by)
          {
              contributions(repository(repo), breaks = breaks, by = by)
          }
)

setMethod('contributions',
          signature(repo = 'git_repository'),
          function (repo, breaks, by)
          {
              breaks <- match.arg(breaks)
              by <- match.arg(by)

              df <- as(repo, 'data.frame')
              df$when <- as.POSIXct(cut(df$when, breaks = breaks))

              if(identical(by, 'commits')) {
                  df <- as.data.frame(table(df$when))
                  names(df) <- c('when', 'n')
                  df$when <- as.Date(df$when)
                  return(df)
              }

              ## Summarise the results
              df_summary <- df %.%
                  group_by(name, when) %.%
                  summarise(counts = n()) %.%
                  arrange(when)

              df_melted <-  melt(dcast(df_summary, name ~ when, value.var = "counts"), id.var = "name")
              df_melted$variable <- as.Date(df_melted$variable)
              names(df_melted)[2:3] <- c("when", "counts")

              df_melted
          }
)