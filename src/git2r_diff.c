/*
 *  git2r, R bindings to the libgit2 library.
 *  Copyright (C) 2013-2017 The git2r contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2,
 *  as published by the Free Software Foundation.
 *
 *  git2r is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "git2r_arg.h"
#include "git2r_diff.h"
#include "git2r_error.h"
#include "git2r_tree.h"
#include "git2r_repository.h"

#include "git2.h"
#include "buffer.h"
#include "diff.h"

#include <stdlib.h>
#include <string.h>

int git2r_diff_count(git_diff *diff, size_t *num_files,
		     size_t *max_hunks, size_t *max_lines);
int git2r_diff_format_to_r(git_diff *diff, SEXP dest);
int git2r_diff_print(git_diff *diff, SEXP filename, SEXP* buf_r);
SEXP git2r_diff_index_to_wd(SEXP repo, SEXP filename);
SEXP git2r_diff_head_to_index(SEXP repo, SEXP filename);
SEXP git2r_diff_tree_to_wd(SEXP tree, SEXP filename);
SEXP git2r_diff_tree_to_index(SEXP tree, SEXP filename);
SEXP git2r_diff_tree_to_tree(SEXP tree1, SEXP tree2, SEXP filename);

/**
 * Diff
 *
 * Setting index to TRUE is essentially like supplying the --cached
 * option to command line git.
 *
 * - If tree1 is NULL and index is FALSE, then we compare the working
 *   directory to the index. (tree2 must be NULL in this case.)
 * - If tree1 is NULL and index is TRUE, then we compare the index
 *   to HEAD. (tree2 must be NULL in this case.)
 * - If tree1 is not NULL and tree2 is NULL, and index is FALSE,
 *   then we compare the working directory to the tree1.
 *   (repo must be NULL in this case.)
 * - If tree1 is not NULL and tree2 is NULL, and index is TRUE,
 *   then we compare the index to tree1. (repo must be NULL.)
 * - If tree1 is not NULL and tree2 is not NULL, then we compare
 *   tree1 to tree2. (repo must be NULL, index is ignored in this case).
 *
 * @param repo Repository.
 * @param tree1 The first tree to compare.
 * @param tree2 The second tree to compare.
 * @param index Whether to compare to the index.
 * @param filename Determines where to write the diff. If filename is
 * R_NilValue, then the diff is written to a S4 class git_diff
 * object. If filename is a character vector of length 0, then the
 * diff is written to a character vector. If filename is a character
 * vector of length one with non-NA value, the diff is written to a
 * file with name filename (the file is overwritten if it exists).
 * @return A S4 class git_diff object if filename equals R_NilValue. A
 * character vector with diff if filename has length 0. Oterwise NULL.
 */
SEXP git2r_diff(SEXP repo, SEXP tree1, SEXP tree2, SEXP index, SEXP filename)
{
    int c_index;

    if (git2r_arg_check_logical(index))
        git2r_error(__func__, NULL, "'index'", git2r_err_logical_arg);

    c_index = LOGICAL(index)[0];

    if (Rf_isNull(tree1) && ! c_index) {
	if (!Rf_isNull(tree2))
	    git2r_error(__func__, NULL, git2r_err_diff_arg, NULL);
	return git2r_diff_index_to_wd(repo, filename);
    } else if (Rf_isNull(tree1) && c_index) {
	if (!Rf_isNull(tree2))
	    git2r_error(__func__, NULL, git2r_err_diff_arg, NULL);
	return git2r_diff_head_to_index(repo, filename);
    } else if (!Rf_isNull(tree1) && Rf_isNull(tree2) && ! c_index) {
	if (!Rf_isNull(repo))
	    git2r_error(__func__, NULL, git2r_err_diff_arg, NULL);
	return git2r_diff_tree_to_wd(tree1, filename);
    } else if (!Rf_isNull(tree1) && Rf_isNull(tree2) && c_index) {
	if (!Rf_isNull(repo))
	    git2r_error(__func__, NULL, git2r_err_diff_arg, NULL);
	return git2r_diff_tree_to_index(tree1, filename);
    } else {
	if (!Rf_isNull(repo))
	    git2r_error(__func__, NULL, git2r_err_diff_arg, NULL);
	return git2r_diff_tree_to_tree(tree1, tree2, filename);
    }
}

/**
 * Create a diff between the repository index and the workdir
 * directory.
 *
 * @param repo S4 class git_repository
 * @param filename Determines where to write the diff. If filename is
 * R_NilValue, then the diff is written to a S4 class git_diff
 * object. If filename is a character vector of length 0, then the
 * diff is written to a character vector. If filename is a character
 * vector of length one with non-NA value, the diff is written to a
 * file with name filename (the file is overwritten if it exists).
 * @return A S4 class git_diff object if filename equals R_NilValue. A
 * character vector with diff if filename has length 0. Oterwise NULL.
 */
SEXP git2r_diff_index_to_wd(SEXP repo, SEXP filename)
{
    int err, nprotect = 0;
    git_repository *repository = NULL;
    git_diff *diff = NULL;
    SEXP result = R_NilValue;

    if (git2r_arg_check_filename(filename))
        git2r_error(__func__, NULL, "'filename'", git2r_err_filename_arg);

    repository = git2r_repository_open(repo);
    if (!repository)
        git2r_error(__func__, NULL, git2r_err_invalid_repository, NULL);

    err = git_diff_index_to_workdir(&diff,
				    repository,
				    /*index=*/ NULL,
				    /*opts=*/ NULL);

    if (err)
	goto cleanup;

    if (Rf_isNull(filename)) {
        SEXP s_new = Rf_install("new");
        SEXP s_old = Rf_install("old");

        PROTECT(result = NEW_OBJECT(MAKE_CLASS("git_diff")));
        nprotect++;
        SET_SLOT(result, s_old, Rf_mkString("index"));
        SET_SLOT(result, s_new, Rf_mkString("workdir"));
        err = git2r_diff_format_to_r(diff, result);
    } else if (0 == Rf_length(filename)) {
        git_buf buf = GIT_BUF_INIT;

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_buf,
            &buf);
        if (0 == err) {
            PROTECT(result = Rf_mkString(buf.ptr));
            nprotect++;
        }

        git_buf_free(&buf);
    } else {
        FILE *fp = fopen(CHAR(STRING_ELT(filename, 0)), "w+");

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_file_handle,
            fp);

        if (fp)
            fclose(fp);
    }

cleanup:
    if (diff)
	git_diff_free(diff);

    if (repository)
        git_repository_free(repository);

    if (nprotect)
        UNPROTECT(nprotect);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Create a diff between head and repository index
 *
 * @param repo S4 class git_repository
 * @param filename Determines where to write the diff. If filename is
 * R_NilValue, then the diff is written to a S4 class git_diff
 * object. If filename is a character vector of length 0, then the
 * diff is written to a character vector. If filename is a character
 * vector of length one with non-NA value, the diff is written to a
 * file with name filename (the file is overwritten if it exists).
 * @return A S4 class git_diff object if filename equals R_NilValue. A
 * character vector with diff if filename has length 0. Oterwise NULL.
 */
SEXP git2r_diff_head_to_index(SEXP repo, SEXP filename)
{
    int err, nprotect = 0;
    git_repository *repository = NULL;
    git_diff *diff = NULL;
    git_object *obj = NULL;
    git_tree *head = NULL;
    SEXP result = R_NilValue;

    if (git2r_arg_check_filename(filename))
        git2r_error(__func__, NULL, "'filename'", git2r_err_filename_arg);

    repository = git2r_repository_open(repo);
    if (!repository)
        git2r_error(__func__, NULL, git2r_err_invalid_repository, NULL);

    err = git_revparse_single(&obj, repository, "HEAD^{tree}");
    if (err)
	goto cleanup;

    err = git_tree_lookup(&head, repository, git_object_id(obj));
    if (err)
	goto cleanup;

    err = git_diff_tree_to_index(
        &diff,
        repository,
        head,
        /* index= */ NULL,
        /* opts = */ NULL);
    if (err)
	goto cleanup;

    if (Rf_isNull(filename)) {
        SEXP s_new = Rf_install("new");
        SEXP s_old = Rf_install("old");

        /* TODO: object instead of HEAD string */
        PROTECT(result = NEW_OBJECT(MAKE_CLASS("git_diff")));
        nprotect++;
        SET_SLOT(result, s_old, Rf_mkString("HEAD"));
        SET_SLOT(result, s_new, Rf_mkString("index"));
        err = git2r_diff_format_to_r(diff, result);
    } else if (0 == Rf_length(filename)) {
        git_buf buf = GIT_BUF_INIT;

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_buf,
            &buf);
        if (0 == err) {
            PROTECT(result = Rf_mkString(buf.ptr));
            nprotect++;
        }

        git_buf_free(&buf);
    } else {
        FILE *fp = fopen(CHAR(STRING_ELT(filename, 0)), "w+");

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_file_handle,
            fp);

        if (fp)
            fclose(fp);
    }

cleanup:
    if (head)
	git_tree_free(head);

    if (obj)
	git_object_free(obj);

    if (diff)
	git_diff_free(diff);

    if (repository)
        git_repository_free(repository);

    if (nprotect)
        UNPROTECT(nprotect);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Create a diff between a tree and the working directory
 *
 * @param tree S4 class git_tree
 * @param filename Determines where to write the diff. If filename is
 * R_NilValue, then the diff is written to a S4 class git_diff
 * object. If filename is a character vector of length 0, then the
 * diff is written to a character vector. If filename is a character
 * vector of length one with non-NA value, the diff is written to a
 * file with name filename (the file is overwritten if it exists).
 * @return A S4 class git_diff object if filename equals R_NilValue. A
 * character vector with diff if filename has length 0. Oterwise NULL.
 */
SEXP git2r_diff_tree_to_wd(SEXP tree, SEXP filename)
{
    int err, nprotect = 0;
    git_repository *repository = NULL;
    git_diff *diff = NULL;
    git_object *obj = NULL;
    git_tree *c_tree = NULL;
    SEXP sha;
    SEXP result = R_NilValue;
    SEXP repo;

    if (git2r_arg_check_tree(tree))
        git2r_error(__func__, NULL, "'tree'", git2r_err_tree_arg);
    if (git2r_arg_check_filename(filename))
        git2r_error(__func__, NULL, "'filename'", git2r_err_filename_arg);

    repo = GET_SLOT(tree, Rf_install("repo"));
    repository = git2r_repository_open(repo);
    if (!repository)
        git2r_error(__func__, NULL, git2r_err_invalid_repository, NULL);

    sha = GET_SLOT(tree, Rf_install("sha"));
    err = git_revparse_single(&obj, repository, CHAR(STRING_ELT(sha, 0)));
    if (err)
	goto cleanup;

    err = git_tree_lookup(&c_tree, repository, git_object_id(obj));
    if (err)
	goto cleanup;

    err = git_diff_tree_to_workdir(&diff, repository, c_tree, /* opts = */ NULL);
    if (err)
	goto cleanup;

    if (Rf_isNull(filename)) {
        SEXP s_new = Rf_install("new");

        PROTECT(result = NEW_OBJECT(MAKE_CLASS("git_diff")));
        nprotect++;
        SET_SLOT(result, Rf_install("old"), tree);
        SET_SLOT(result, s_new, Rf_mkString("workdir"));
        err = git2r_diff_format_to_r(diff, result);
    } else if (0 == Rf_length(filename)) {
        git_buf buf = GIT_BUF_INIT;

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_buf,
            &buf);
        if (0 == err) {
            PROTECT(result = Rf_mkString(buf.ptr));
            nprotect++;
        }

        git_buf_free(&buf);
    } else {
        FILE *fp = fopen(CHAR(STRING_ELT(filename, 0)), "w+");

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_file_handle,
            fp);

        if (fp)
            fclose(fp);
    }

cleanup:
    if (diff)
        git_diff_free(diff);

    if (c_tree)
	git_tree_free(c_tree);

    if (obj)
	git_object_free(obj);

    if (repository)
	git_repository_free(repository);

    if (nprotect)
        UNPROTECT(nprotect);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Create a diff between a tree and repository index
 *
 * @param tree S4 class git_tree
 * @param filename Determines where to write the diff. If filename is
 * R_NilValue, then the diff is written to a S4 class git_diff
 * object. If filename is a character vector of length 0, then the
 * diff is written to a character vector. If filename is a character
 * vector of length one with non-NA value, the diff is written to a
 * file with name filename (the file is overwritten if it exists).
 * @return A S4 class git_diff object if filename equals R_NilValue. A
 * character vector with diff if filename has length 0. Oterwise NULL.
 */
SEXP git2r_diff_tree_to_index(SEXP tree, SEXP filename)
{
    int err, nprotect = 0;
    git_repository *repository = NULL;
    git_diff *diff = NULL;
    git_object *obj = NULL;
    git_tree *c_tree = NULL;
    SEXP sha;
    SEXP result = R_NilValue;
    SEXP repo;

    if (git2r_arg_check_tree(tree))
        git2r_error(__func__, NULL, "'tree'", git2r_err_tree_arg);
    if (git2r_arg_check_filename(filename))
        git2r_error(__func__, NULL, "'filename'", git2r_err_filename_arg);

    repo = GET_SLOT(tree, Rf_install("repo"));
    repository = git2r_repository_open(repo);
    if (!repository)
        git2r_error(__func__, NULL, git2r_err_invalid_repository, NULL);

    sha = GET_SLOT(tree, Rf_install("sha"));
    err = git_revparse_single(&obj, repository, CHAR(STRING_ELT(sha, 0)));
    if (err)
	goto cleanup;

    err = git_tree_lookup(&c_tree, repository, git_object_id(obj));
    if (err)
	goto cleanup;

    err = git_diff_tree_to_index(
        &diff,
        repository,
        c_tree,
        /* index= */ NULL,
        /* opts= */ NULL);
    if (err)
	goto cleanup;

    if (Rf_isNull(filename)) {
        SEXP s_new = Rf_install("new");

        PROTECT(result = NEW_OBJECT(MAKE_CLASS("git_diff")));
        nprotect++;
        SET_SLOT(result, Rf_install("old"), tree);
        SET_SLOT(result, s_new, Rf_mkString("index"));
        err = git2r_diff_format_to_r(diff, result);
    } else if (0 == Rf_length(filename)) {
        git_buf buf = GIT_BUF_INIT;

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_buf,
            &buf);
        if (0 == err) {
            PROTECT(result = Rf_mkString(buf.ptr));
            nprotect++;
        }

        git_buf_free(&buf);
    } else {
        FILE *fp = fopen(CHAR(STRING_ELT(filename, 0)), "w+");

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_file_handle,
            fp);

        if (fp)
            fclose(fp);
    }

cleanup:
    if (diff)
        git_diff_free(diff);

    if (c_tree)
	git_tree_free(c_tree);

    if (obj)
	git_object_free(obj);

    if (repository)
	git_repository_free(repository);

    if (nprotect)
        UNPROTECT(nprotect);

    if (err)
	git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Create a diff with the difference between two tree objects
 *
 * @param tree1 S4 class git_tree
 * @param tree2 S4 class git_tree
 * @param filename Determines where to write the diff. If filename is
 * R_NilValue, then the diff is written to a S4 class git_diff
 * object. If filename is a character vector of length 0, then the
 * diff is written to a character vector. If filename is a character
 * vector of length one with non-NA value, the diff is written to a
 * file with name filename (the file is overwritten if it exists).
 * @return A S4 class git_diff object if filename equals R_NilValue. A
 * character vector with diff if filename has length 0. Oterwise NULL.
 */
SEXP git2r_diff_tree_to_tree(SEXP tree1, SEXP tree2, SEXP filename)
{
    int err, nprotect = 0;
    git_repository *repository = NULL;
    git_diff *diff = NULL;
    git_object *obj1 = NULL, *obj2 = NULL;
    git_tree *c_tree1 = NULL, *c_tree2 = NULL;
    SEXP result = R_NilValue;
    SEXP repo;
    SEXP sha1, sha2;

    if (git2r_arg_check_tree(tree1))
        git2r_error(__func__, NULL, "'tree1'", git2r_err_tree_arg);
    if (git2r_arg_check_tree(tree2))
        git2r_error(__func__, NULL, "'tree2'", git2r_err_tree_arg);
    if (git2r_arg_check_filename(filename))
        git2r_error(__func__, NULL, "'filename'", git2r_err_filename_arg);

    /* We already checked that tree2 is from the same repo, in R */
    repo = GET_SLOT(tree1, Rf_install("repo"));
    repository = git2r_repository_open(repo);
    if (!repository)
        git2r_error(__func__, NULL, git2r_err_invalid_repository, NULL);

    sha1 = GET_SLOT(tree1, Rf_install("sha"));
    err = git_revparse_single(&obj1, repository, CHAR(STRING_ELT(sha1, 0)));
    if (err)
	goto cleanup;

    sha2 = GET_SLOT(tree2, Rf_install("sha"));
    err = git_revparse_single(&obj2, repository, CHAR(STRING_ELT(sha2, 0)));
    if (err)
	goto cleanup;

    err = git_tree_lookup(&c_tree1, repository, git_object_id(obj1));
    if (err)
	goto cleanup;

    err = git_tree_lookup(&c_tree2, repository, git_object_id(obj2));
    if (err)
	goto cleanup;

    err = git_diff_tree_to_tree(
        &diff,
        repository,
        c_tree1,
        c_tree2,
        /* opts= */ NULL);
    if (err)
	goto cleanup;

    if (Rf_isNull(filename)) {
        PROTECT(result = NEW_OBJECT(MAKE_CLASS("git_diff")));
        nprotect++;
        SET_SLOT(result, Rf_install("old"), tree1);
        SET_SLOT(result, Rf_install("new"), tree2);
        err = git2r_diff_format_to_r(diff, result);
    } else if (0 == Rf_length(filename)) {
        git_buf buf = GIT_BUF_INIT;

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_buf,
            &buf);
        if (0 == err) {
            PROTECT(result = Rf_mkString(buf.ptr));
            nprotect++;
        }

        git_buf_free(&buf);
    } else {
        FILE *fp = fopen(CHAR(STRING_ELT(filename, 0)), "w+");

        err = git_diff_print(
            diff,
            GIT_DIFF_FORMAT_PATCH,
            git_diff_print_callback__to_file_handle,
            fp);

        if (fp)
            fclose(fp);
    }

cleanup:
    if (diff)
        git_diff_free(diff);

    if (c_tree1)
	git_tree_free(c_tree1);

    if (c_tree2)
	git_tree_free(c_tree2);

    if (obj1)
	git_object_free(obj1);

    if (obj2)
	git_object_free(obj2);

    if (repository)
	git_repository_free(repository);

    if (nprotect)
        UNPROTECT(nprotect);

    if (err)
	git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Data structure to hold the information when counting diff objects.
 */
typedef struct {
    size_t num_files;
    size_t max_hunks;
    size_t max_lines;
    size_t num_hunks;
    size_t num_lines;
} git2r_diff_count_payload;

/**
 * Callback per file in the diff
 *
 * @param delta A pointer to the delta data for the file
 * @param progress Goes from 0 to 1 over the diff
 * @param payload A pointer to the git2r_diff_count_payload data structure
 * @return 0
 */
int git2r_diff_count_file_cb(const git_diff_delta *delta,
			     float progress,
			     void *payload)
{
    git2r_diff_count_payload *n = payload;

    GIT_UNUSED(delta);
    GIT_UNUSED(progress);

    n->num_files += 1;
    n->num_hunks = n->num_lines = 0;
    return 0;
}

/**
 * Callback per hunk in the diff
 *
 * @param delta A pointer to the delta data for the file
 * @param hunk A pointer to the structure describing a hunk of a diff
 * @param payload A pointer to the git2r_diff_count_payload data structure
 * @return 0
 */
int git2r_diff_count_hunk_cb(const git_diff_delta *delta,
			     const git_diff_hunk *hunk,
			     void *payload)
{
    git2r_diff_count_payload *n = payload;

    GIT_UNUSED(delta);
    GIT_UNUSED(hunk);

    n->num_hunks += 1;
    if (n->num_hunks > n->max_hunks) { n->max_hunks = n->num_hunks; }
    n->num_lines = 0;
    return 0;
}

/**
 * Callback per text diff line
 *
 * @param delta A pointer to the delta data for the file
 * @param hunk A pointer to the structure describing a hunk of a diff
 * @param line A pointer to the structure describing a line (or data
 * span) of a diff.
 * @param payload A pointer to the git2r_diff_count_payload data structure
 * @return 0
 */
int git2r_diff_count_line_cb(const git_diff_delta *delta,
			     const git_diff_hunk *hunk,
			     const git_diff_line *line,
			     void *payload)
{
    git2r_diff_count_payload *n = payload;

    GIT_UNUSED(delta);
    GIT_UNUSED(hunk);
    GIT_UNUSED(line);

    n->num_lines += 1;
    if (n->num_lines > n->max_lines) { n->max_lines = n->num_lines; }
    return 0;
}


/**
 *  Count diff objects
 *
 * @param diff Pointer to the diff
 * @param num_files Pointer where to save the number of files
 * @param max_hunks Pointer where to save the maximum number of hunks
 * @param max_lines Pointer where to save the maximum number of lines
 * @return 0 if OK, else -1
 */
int git2r_diff_count(git_diff *diff,
                     size_t *num_files,
		     size_t *max_hunks,
                     size_t *max_lines)
{
    int err;
    git2r_diff_count_payload n = { 0, 0, 0 };

    err = git_diff_foreach(diff,
			   git2r_diff_count_file_cb,
                           /* binary_cb */ NULL,
			   git2r_diff_count_hunk_cb,
			   git2r_diff_count_line_cb,
			   /* payload= */ (void*) &n);

    if (err)
	return -1;

    *num_files = n.num_files;
    *max_hunks = n.max_hunks;
    *max_lines = n.max_lines;

    return 0;
}

/**
 * Data structure to hold the callback information when generating
 * diff objects.
 */
typedef struct {
    SEXP result;
    SEXP hunk_tmp;
    SEXP line_tmp;
    size_t file_ptr, hunk_ptr, line_ptr;
} git2r_diff_payload;

int git2r_diff_get_hunk_cb(const git_diff_delta *delta,
			   const git_diff_hunk *hunk,
			   void *payload);

int git2r_diff_get_line_cb(const git_diff_delta *delta,
			   const git_diff_hunk *hunk,
			   const git_diff_line *line,
			   void *payload);

/**
 * Callback per file in the diff
 *
 * @param delta A pointer to the delta data for the file
 * @param progress Goes from 0 to 1 over the diff
 * @param payload A pointer to the git2r_diff_payload data structure
 * @return 0
 */
int git2r_diff_get_file_cb(const git_diff_delta *delta,
			   float progress,
			   void *payload)
{
    git2r_diff_payload *p = (git2r_diff_payload *) payload;

    GIT_UNUSED(progress);

    /* Save previous hunk's lines in hunk_tmp, we just call the
       hunk callback, with a NULL hunk */
    git2r_diff_get_hunk_cb(delta, /* hunk= */ 0, payload);

    /* Save the previous file's hunks from the hunk_tmp
       temporary storage. */
    if (p->file_ptr != 0) {
        SEXP hunks;
        SEXP s_hunks = Rf_install("hunks");
	size_t len=p->hunk_ptr, i;

	SET_SLOT(
            VECTOR_ELT(p->result, p->file_ptr-1),
            s_hunks,
            hunks = Rf_allocVector(VECSXP, p->hunk_ptr));
	for (i = 0; i < len ; i++)
	    SET_VECTOR_ELT(hunks, i, VECTOR_ELT(p->hunk_tmp, i));
    }

    /* OK, ready for next file, if any */
    if (delta) {
	SEXP file_obj;
        SEXP s_new_file = Rf_install("new_file");
        SEXP s_old_file = Rf_install("old_file");

        PROTECT(file_obj = NEW_OBJECT(MAKE_CLASS("git_diff_file")));
	SET_VECTOR_ELT(p->result, p->file_ptr, file_obj);
	SET_SLOT(file_obj, s_old_file, Rf_mkString(delta->old_file.path));
	SET_SLOT(file_obj, s_new_file, Rf_mkString(delta->new_file.path));
        UNPROTECT(1);

	p->file_ptr++;
	p->hunk_ptr = 0;
	p->line_ptr = 0;
    }

    return 0;
}

/**
 * Process a hunk
 *
 * First we save the previous hunk, if there was one. Then create an
 * empty hunk (i.e. without any lines) and put it in the result.
 *
 * @param delta A pointer to the delta data for the file
 * @param hunk A pointer to the structure describing a hunk of a diff
 * @param payload Pointer to a git2r_diff_payload data structure
 * @return 0
 */
int git2r_diff_get_hunk_cb(const git_diff_delta *delta,
			   const git_diff_hunk *hunk,
			   void *payload)
{
    git2r_diff_payload *p = (git2r_diff_payload *) payload;

    GIT_UNUSED(delta);

    /* Save previous hunk's lines in hunk_tmp, from the line_tmp
       temporary storage. */
    if (p->hunk_ptr != 0) {
	SEXP lines;
	size_t len=p->line_ptr, i;
        SEXP s_lines = Rf_install("lines");

        PROTECT(lines = Rf_allocVector(VECSXP, p->line_ptr));
	SET_SLOT(VECTOR_ELT(p->hunk_tmp, p->hunk_ptr-1), s_lines, lines);
	for (i = 0; i < len; i++)
	    SET_VECTOR_ELT(lines, i, VECTOR_ELT(p->line_tmp, i));
        UNPROTECT(1);
    }

    /* OK, ready for the next hunk, if any */
    if (hunk) {
	SEXP hunk_obj;
        SEXP s_old_start = Rf_install("old_start");
        SEXP s_old_lines = Rf_install("old_lines");
        SEXP s_new_start = Rf_install("new_start");
        SEXP s_new_lines = Rf_install("new_lines");
        SEXP s_header = Rf_install("header");

        PROTECT(hunk_obj = NEW_OBJECT(MAKE_CLASS("git_diff_hunk")));
	SET_VECTOR_ELT(p->hunk_tmp, p->hunk_ptr, hunk_obj);
	SET_SLOT(hunk_obj, s_old_start, Rf_ScalarInteger(hunk->old_start));
	SET_SLOT(hunk_obj, s_old_lines, Rf_ScalarInteger(hunk->old_lines));
	SET_SLOT(hunk_obj, s_new_start, Rf_ScalarInteger(hunk->new_start));
	SET_SLOT(hunk_obj, s_new_lines, Rf_ScalarInteger(hunk->new_lines));
	SET_SLOT(hunk_obj, s_header, Rf_mkString(hunk->header));
        UNPROTECT(1);

	p->hunk_ptr += 1;
	p->line_ptr = 0;
    }

    return 0;
}

/**
 * Process a line
 *
 * This is easy, just populate a git_diff_line object and
 * put it in the temporary hunk.
 *
 * @param delta A pointer to the delta data for the file
 * @param hunk A pointer to the structure describing a hunk of a diff
 * @param line A pointer to the structure describing a line (or data
 * span) of a diff.
 * @param payload Pointer to a git2r_diff_payload data structure
 * @return 0
 */
int git2r_diff_get_line_cb(const git_diff_delta *delta,
			   const git_diff_hunk *hunk,
			   const git_diff_line *line,
			   void *payload)
{
    git2r_diff_payload *p = (git2r_diff_payload *) payload;
    static char short_buffer[200];
    char *buffer = short_buffer;
    SEXP line_obj;
    SEXP s_origin = Rf_install("origin");
    SEXP s_old_lineno = Rf_install("old_lineno");
    SEXP s_new_lineno = Rf_install("new_lineno");
    SEXP s_num_lines = Rf_install("num_lines");
    SEXP s_content = Rf_install("content");

    GIT_UNUSED(delta);
    GIT_UNUSED(hunk);

    PROTECT(line_obj = NEW_OBJECT(MAKE_CLASS("git_diff_line")));
    SET_VECTOR_ELT(p->line_tmp, p->line_ptr++, line_obj);

    SET_SLOT(line_obj, s_origin, Rf_ScalarInteger(line->origin));
    SET_SLOT(line_obj, s_old_lineno, Rf_ScalarInteger(line->old_lineno));
    SET_SLOT(line_obj, s_new_lineno, Rf_ScalarInteger(line->new_lineno));
    SET_SLOT(line_obj, s_num_lines, Rf_ScalarInteger(line->num_lines));

    if (line->content_len > sizeof(buffer))
	buffer = malloc(line->content_len+1);
    memcpy(buffer, line->content, line->content_len);
    buffer[line->content_len] = 0;

    SET_SLOT(line_obj, s_content, Rf_mkString(buffer));

    if (buffer != short_buffer)
	free(buffer);

    UNPROTECT(1);

    return 0;
}

/**
 * Format a diff as an R object
 *
 * libgit2 has callbacks to walk over the files, hunks and line
 * of a diff. This means that we need to walk over the diff twice,
 * if we don't want to reallocate our lists over and over again (or write a
 * smart list that preallocates memory).
 *
 * So we walk over it first and calculate the maximum number of
 * hunks in a file, and the maximum number of lines in a hunk.
 *
 * Then in the second walk, we have a correspondingly allocated
 * list that we use for temporary storage.
 *
 * @param diff Pointer to the diff
 * @param dest The S4 class git_diff to hold the formated diff
 * @return 0 if OK, else error code
 */
int git2r_diff_format_to_r(git_diff *diff, SEXP dest)
{
    int err;
    git2r_diff_payload payload = { /* result=   */ R_NilValue,
                                   /* hunk_tmp= */ R_NilValue,
                                   /* line_tmp= */ R_NilValue,
                                   /* file_ptr= */ 0,
                                   /* hunk_ptr= */ 0,
                                   /* line_ptr= */ 0 };

    size_t num_files, max_hunks, max_lines;

    err = git2r_diff_count(diff, &num_files, &max_hunks, &max_lines);

    if (err)
        return err;

    PROTECT(payload.result = Rf_allocVector(VECSXP, num_files));
    SET_SLOT(dest, Rf_install("files"), payload.result);
    PROTECT(payload.hunk_tmp = Rf_allocVector(VECSXP, max_hunks));
    PROTECT(payload.line_tmp = Rf_allocVector(VECSXP, max_lines));

    err = git_diff_foreach(
        diff,
        git2r_diff_get_file_cb,
        /* binary_cb */ NULL,
        git2r_diff_get_hunk_cb,
        git2r_diff_get_line_cb,
        &payload);
    if (GIT_OK == err) {
        /* Need to call them once more, to put in the last lines/hunks/files. */
        err = git2r_diff_get_file_cb(
            /* delta=    */ NULL,
            /* progress= */ 100,
            &payload);
    }

    UNPROTECT(3);

    return err;
}
