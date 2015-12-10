#!/usr/bin/env Rscript

library(dplyr,    quietly=TRUE, warn.conflicts = FALSE)
library(reshape2, quietly=TRUE, warn.conflicts = FALSE)
library(ggplot2,  quietly=TRUE, warn.conflicts = FALSE)

## Args and command line nonsense.

exit <- function(i) {
    if (i != 0) {
        cat("Usage: jon2.r --in=this/path{.csv} [--out=that/path.ext]
  where the string given to the --in flag is one of:
        - a path to an experiment's CSV file, or
        - a path to a directory containing many such CSV files
    and the acceptable `ext' format for the output are:
          { jpeg, eps, ps, tex, pdf, tiff, png, bmp, svg }.
     If --out is omitted, generates a jpeg \"this/path.jpeg\" for each
          input \"this/path.csv\".
")}
    q(save="no", runLast = FALSE, status = i)
}

input <- "" ; output <- ""
exts = c("jpeg", "eps", "ps", "tex", "pdf", "tiff", "png", "bmp", "svg")
ptheme = theme( axis.text.x  = element_text(size=18)
              , axis.text.y  = element_text(size=18)
              , axis.title.x = element_text(size=18)
              , axis.title.y = element_text(size=18)
              , strip.text.x = element_text(size=18)
              , legend.title = element_blank()
              , legend.text  = element_text(size=18)
              , plot.title   = element_text(size=26, face="bold", vjust=2))
title <- "Configuration Performance over Time"
ylab <- "Weighted Time (lower is better)"
xlab <- "# Iterations"
plabels = labs(x=xlab, y=ylab, title=title)


outpath <- function(input) {
    splt <- strsplit(input, ".", fixed=TRUE)
    path <- paste(head(splt[[1]], -1), collapse=".")
    if (is.null(output) || length(output) == 0) {
        return(paste0(path, ".", exts[1]))
    } else if (output %in% exts) {
        return(paste0(path, ".", output))
    } else {
        outext <- tail(strsplit(output, ".")[[1]], 1)
        if (!(outext %in% exts)) {
            cat(paste0("Output type ",
                       outext,
                       " not known, expected file with extension in:\n"))
            cat(paste("{", paste(exts, collapse=", "), "}\n\n"))
            exit(2)
        } else {
            return(output)
        }
    }
}


rename_strat <- function(x)
  if (x == "0") {
    "Random"
  } else if (x == "1") {
    "Sim Anneal"
  } else if (x == "2") {
    "Beam Search"
  } else {
    "UNKNOWN"
  }

cap <- function(str)
    paste0(toupper(substr(str, 1, 1)), substr(str, 2, nchar(str)))

# Renders and returns the graph for an individual experiment, given
# the path of its output CSV.
render_graph <- function(in1, in2, in3) {
    tryCatch({
    results1 <- mutate(read.csv(in1), strategy = "Nelder-Mead")
    results2 <- mutate(read.csv(in2), strategy = "Beam")
    results3 <- mutate(read.csv(in3), strategy = "Sim Anneal")
    all <- rbind(results1, rbind(results2, results3))

    p <- ggplot(all,
                aes( x = id
                   , y = result
                   , group = as.factor(strategy)
                   , color = as.factor(strategy))) +
         geom_point(size=3) +
         plabels + ptheme

    output <- "merged.jpg"
    ggsave(p, file = output, width = 12, height = 7)
    cat(paste0("rendered ", output, "\n"))
    }, error = function(e) {
        cat(paste0( "An error occurred rendering graph for '"
                  , input
                  , "':\n"
                  , e$message
                  , "\n"))
    })
}

invisible(render_graph("nm.csv", "beam.csv", "sim_anneal.csv"))

exit(0)
