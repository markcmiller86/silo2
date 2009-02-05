/*-------------------------------------------------------------------------
 * Copyright (C) 1996   The Regents of the University of California.
 *                      All rights reserved.
 *
 * This work was produced at the University of California,  Lawrence Liver-
 * more National Laboratory  (UC  LLNL)  under  contract no.  W-7405-ENG-48
 * (Contract 48)  between the U.S.  Department  of  Energy  (DOE)  and  The
 * Regents of the University of California  (University) for  the operation
 * of UC LLNL.   Copyright is reserved  to the University  for purposes  of
 * controlled dissemination, commercialization through formal licensing, or
 * other disposition under terms of Contract 48;  DOE policies, regulations
 * and orders; and U.S. statutes.  The rights of the Federal Government are
 * reserved under  Contract 48 subject  to the restrictions  agreed upon by
 * DOE and University.
 *
 *                            DISCLAIMER
 *
 * This software was prepared as an account of work  sponsored by an agency
 * of the United States  Government.  Neither the United  States Government
 * nor the University of  California nor any  of their employees, makes any
 * warranty, express or implied, or assumes  any liability or responsiblity
 * for the accuracy, completeness, or usefullness of any information, appa-
 * ratus,  product,  or process disclosed, or represents that its use would
 * not infringe  privately owned rights.   Reference herein to any specific
 * commercial products, process, or service by trade name, trademark, manu-
 * facturer, or otherwise, does not necessarily constitute or imply its en-
 * dorsement,  recommendation,  or favoring by the United States Government
 * or the University of California.   The views and opinions of authors ex-
 * pressed herein do not necessarily  state or reflect those  of the United
 * States Government or the University of California, and shall not be used
 * for advertising or product endorsement purposes.
 *
 *-------------------------------------------------------------------------
 *
 * Created:             stc.c
 *                      Dec  6 1996
 *                      Robb Matzke <matzke@viper.llnl.gov>
 *
 * Purpose:             The structure type class.
 *
 * Modifications:       
 *
 *-------------------------------------------------------------------------
 */
#include <assert.h>
#include <browser.h>
#define MYCLASS(X)      ((obj_stc_t*)(X))

typedef struct obj_stc_t {
   obj_pub_t    pub;
   char         *name;                  /*name of structure or ""       */
   int          ncomps;                 /*number of components used     */
   int          acomps;                 /*number of components allocated*/
   int          *offset;                /*offset for each component     */
   obj_t        *sub;                   /*type of each component        */
   char         **compname;             /*name of each component        */
   void         (*walk1)(obj_t,void*,int,walk_t*); /*walk1 override     */
   int          (*walk2)(obj_t,void*,obj_t,void*,walk_t*);/*walk2 override*/
} obj_stc_t;

class_t         C_STC;
static obj_t    stc_new (va_list);
static obj_t    stc_copy (obj_t, int);
static obj_t    stc_dest (obj_t);
static obj_t    stc_feval (obj_t);
static obj_t    stc_apply (obj_t, obj_t);
static void     stc_walk1 (obj_t, void*, int, walk_t*);
static int      stc_walk2 (obj_t, void*, obj_t, void*, walk_t*);
static void     stc_print (obj_t, out_t*);
static obj_t    stc_deref (obj_t, int, obj_t*);
static char *   stc_name (obj_t);
static obj_t    stc_bind (obj_t, void*);

#define STRUCT(ST)      STRUCT2(ST,#ST)

#define STRUCT2(ST_R,ST_F) {                                                  \
   ST_R _tmp ;                                                                \
   lex_t *_li=NULL ;                                                          \
   obj_t _in=NIL, _out=NIL ;                                                  \
   char *_tname = ST_F ;                                                      \
   obj_t _t = obj_new (C_STC, ST_F, NULL);                                    \
   obj_stc_t *_tt = (obj_stc_t*)_t;
   
#define COMP(FIELD,TYPE)        COMP2(FIELD,#FIELD,TYPE)

#define COMP2(FIELD_R,FIELD_F,TYPE)                                           \
   _li = lex_string (TYPE);                                                   \
   _in = parse_stmt (_li, false);                                             \
   _li = lex_close (_li);                                                     \
   _out = obj_eval (_in);                                                     \
   _in = obj_dest (_in);                                                      \
   assert (_out);                                                             \
   COMP3 (FIELD_R, FIELD_F, _out);

#define COMP3(FIELD_R,FIELD_F,TYPE)                                           \
   stc_add (_t, TYPE, ((char*)(&(_tmp.FIELD_R))-(char*)(&_tmp)), FIELD_F);    \
   prim_set_io_assoc(_tt->sub[_tt->ncomps-1], NULL); /*for cc warning*/

#define IOASSOC(ASSOC)                                                        \
   assert (_tt->ncomps-1>=0);                                                 \
   assert (_tt->sub[_tt->ncomps-1]);                                          \
   _in = prim_set_io_assoc (_tt->sub[_tt->ncomps-1], ASSOC);                  \
   assert (_in) ;                                                             \
   _in = NIL;

#define WALK1(FUNC)                                                           \
   assert (_tt);                                                              \
   _tt->walk1 = FUNC

#define WALK2(FUNC)                                                           \
   assert (_tt);                                                              \
   _tt->walk2 = FUNC

#define ESTRUCT                                                               \
  _in = obj_new (C_SYM, _tname);                                              \
  sym_vbind (_in, _t);                                                        \
  obj_dest (_in) ;                                                            \
  _tt=NULL; /*to prevent compiler warning*/                                   \
}

   


/*-------------------------------------------------------------------------
 * Function:    stc_class
 *
 * Purpose:     Initializes the STRUCT class.
 *
 * Return:      Success:        Ptr to the class
 *
 *              Failure:        NULL
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *
 *    Lisa J. Roberts, Mon Nov 22 17:27:53 PST 1999
 *    I changed strdup to safe_strdup.
 *
 *-------------------------------------------------------------------------
 */
class_t
stc_class (void) {

   class_t      cls = calloc (1, sizeof(*cls));

   cls->name = safe_strdup ("STRUCT");
   cls->new = stc_new;
   cls->copy = stc_copy;
   cls->dest = stc_dest;
   cls->feval = stc_feval;
   cls->apply = stc_apply;
   cls->print = stc_print;
   cls->walk1 = stc_walk1;
   cls->walk2 = stc_walk2;
   cls->deref = stc_deref;
   cls->objname = stc_name;
   cls->bind = stc_bind;
   return cls;
}


/*-------------------------------------------------------------------------
 * Function:    stc_new
 *
 * Purpose:     Creates a new struct object with the specified components.
 *              Each following structure component consists of three values:
 *              the type of the component, a byte offset from the beginning
 *              of the structure, and the name of the component.  All are
 *              required.  The argument list must be terminated with a
 *              NULL pointer.
 *
 *              The first argument is an optional structure name.
 *
 * Return:      Success:        Ptr to a new STRUCT object.
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *
 *    Lisa J. Roberts, Mon Nov 22 17:27:53 PST 1999
 *    I changed strdup to safe_strdup.
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_new (va_list ap) {

   obj_stc_t    *self = calloc (1, sizeof(obj_stc_t));
   obj_t        _self = (obj_t)self;
   obj_t        sub;
   int          offset;
   char         *name;

   name = va_arg (ap, char*);
   self->name = safe_strdup (name?name:"");

   for (;;) {
      sub = va_arg (ap, obj_t);
      if (!sub) break;
      offset = va_arg (ap, int);
      name = va_arg (ap, char*);

      stc_add (_self, sub, offset, name);
   }

   return _self;
}


/*-------------------------------------------------------------------------
 * Function:    stc_add
 *
 * Purpose:     Destructively adds another field to a structure.
 *
 * Return:      Success:        SELF
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Jan 31 1997
 *
 * Modifications:
 *
 *    Lisa J. Roberts, Mon Nov 22 17:27:53 PST 1999
 *    I changed strdup to safe_strdup.
 *
 *-------------------------------------------------------------------------
 */
obj_t
stc_add (obj_t _self, obj_t sub, int offset, const char *name) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i;

   /*
    * Does the field already exist?  If yes, replace it with the new
    * definition.
    */
   for (i=0; i<self->ncomps; i++) {
      if (!strcmp(self->compname[i], name)) {
         obj_dest (self->sub[i]);
         self->sub[i] = sub;
         self->offset[i] = offset;
         return _self;
      }
   }

   /*
    * (Re)allocate space for the new component (possibly with malloc
    * for those non-posix systems.
    */
   if (self->ncomps>=self->acomps) {
      self->acomps = MAX (2*self->acomps, 30);
      if (self->sub) {
         self->offset = realloc (self->offset, self->acomps*sizeof(int));
         self->compname = realloc (self->compname, self->acomps*sizeof(char*));
         self->sub = realloc (self->sub, self->acomps*sizeof(obj_t));
      } else {
         self->offset = malloc (self->acomps*sizeof(int));
         self->compname = malloc (self->acomps*sizeof(char*));
         self->sub = malloc (self->acomps*sizeof(obj_t));
      }
      assert (self->offset);
      assert (self->compname);
      assert (self->sub);
   }
         

   /*
    * Create the new component
    */
   i = self->ncomps;
   self->ncomps += 1;

   self->offset[i] = offset;
   self->sub[i] = sub;
   self->compname[i] = safe_strdup (name);
   
   return _self;
}


/*-------------------------------------------------------------------------
 * Function:    stc_copy
 *
 * Purpose:     Copies a structure.
 *
 * Return:      Success:        Ptr to a copy of the structure.
 *
 *              Failure:        abort()
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Jan 22 1997
 *
 * Modifications:
 *
 *    Lisa J. Roberts, Mon Nov 22 17:27:53 PST 1999
 *    I changed strdup to safe_strdup.
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_copy (obj_t _self, int flag) {

   obj_stc_t    *self = MYCLASS(_self);
   obj_stc_t    *retval = NULL;
   obj_t        x;
   int          i;

   if (SHALLOW==flag) {
      for (i=0; i<self->ncomps; i++) {
         x = obj_copy (self->sub[i], SHALLOW);
         assert (x==self->sub[i]);
      }
      retval = self;

   } else {
      retval = calloc (1, sizeof(obj_stc_t));
      retval->walk1 = self->walk1;
      retval->walk2 = self->walk2;
      retval->name = safe_strdup (self->name);
      retval->acomps = self->ncomps;    /*save some memory*/
      retval->sub = malloc (retval->acomps * sizeof(obj_t));
      retval->offset = malloc (retval->acomps * sizeof(int));
      retval->compname = malloc (retval->acomps * sizeof(char*));
      retval->ncomps = self->ncomps;
      
      for (i=0; i<self->ncomps; i++) {
         retval->sub[i] = obj_copy (self->sub[i], DEEP);
         retval->offset[i] = self->offset[i];
         retval->compname[i] = safe_strdup (self->compname[i]);
      }
   }

   return (obj_t)retval;
}


/*-------------------------------------------------------------------------
 * Function:    stc_dest
 *
 * Purpose:     Destroys a structure type object.
 *
 * Return:      Success:        NIL
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *
 *      Robb Matzke, 5 Feb 1997
 *      Frees memory associated with the structure.
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_dest (obj_t _self) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i;

   for (i=0; i<self->ncomps; i++) {
      obj_dest (self->sub[i]);
      if (0==self->pub.ref) {
         if (self->compname[i]) free (self->compname[i]);
      }
   }
   
   if (0==self->pub.ref) {
      free (self->name);
      free (self->sub);
      free (self->offset);
      free (self->compname);
      memset (self, 0, sizeof(obj_stc_t));
   }
   return NIL;
}


/*-------------------------------------------------------------------------
 * Function:    stc_feval
 *
 * Purpose:     The functional value of a type is the type.
 *
 * Return:      Success:        Copy of SELF
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_feval (obj_t _self) {

   return obj_copy (_self, SHALLOW);
}


/*-------------------------------------------------------------------------
 * Function:    stc_apply
 *
 * Purpose:     Applying a structure type to an argument list consisting of
 *              a single SILO database object (SDO) causes the object to
 *              be cast to that type.
 *
 * Return:      Success:        Ptr to a new SDO object with the appropriate
 *                              type.
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_apply (obj_t _self, obj_t args) {

   obj_t        sdo=NIL, retval=NIL;

   if (1!=F_length(args)) {
      out_errorn ("typecast: wrong number of arguments");
      return NIL;
   }

   sdo = obj_eval (cons_head (args));
   retval = sdo_cast (sdo, _self);
   obj_dest (sdo);
   return retval;
}


/*-------------------------------------------------------------------------
 * Function:    stc_print
 *
 * Purpose:     Prints a structure type.
 *
 * Return:      void
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void
stc_print (obj_t _self, out_t *f) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i;

   if (self->name[0]) {
      out_printf (f, "struct %s {\n", self->name);
   } else {
      out_printf (f, "struct {\n");
   }

   out_indent (f);
   if (self->walk1) out_printf (f, "__OVERRIDE_PRINT_FUNCTION__\n");
   if (self->walk2) out_printf (f, "__OVERRIDE_DIFF_FUNCTION__\n");
   for (i=0; i<self->ncomps && !out_brokenpipe(f); i++) {
      out_push (f, self->compname[i]);
      out_printf (f, "+%d ", self->offset[i]);
      obj_print (self->sub[i], f);
      out_nl (f);
      out_pop (f);
   }
   out_undent (f);
   out_puts (f, "}");
}


/*-------------------------------------------------------------------------
 * Function:    stc_walk1_DBdirectory
 *
 * Purpose:     Walks a directory to print the results in a nice format.
 *
 * Return:      void
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Jul 25 1997
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
stc_walk1_DBdirectory (obj_t _self, void *mem, int operation, walk_t *wdata)
{
   DBdirectory  *dir = (DBdirectory *)mem;
   int          width=0, i, old_type=-1, nprint;
   char         buf[64];

   assert (WALK_PRINT==operation);

   for (i=0; i<dir->nsyms; i++) {
      width = MAX (width, strlen(dir->toc[i].name));
   }
   if (0==width) {
      out_errorn ("empty directory");
      return;
   }

   for (i=nprint=0; i<dir->nsyms && !out_brokenpipe(OUT_STDOUT); i++) {
      if (dir->toc[i].type!=old_type) {
         if (nprint) {
            out_nl (OUT_STDOUT);
            out_nl (OUT_STDOUT);
         }
         sprintf (buf, "%s(s)", ObjTypeName[dir->toc[i].type]);
         out_push (OUT_STDOUT, buf);
      }
      out_printf (OUT_STDOUT, " %-*s", width, dir->toc[i].name);
      if (dir->toc[i].type!=old_type) {
         out_pop (OUT_STDOUT);
         old_type = dir->toc[i].type;
      }
      nprint++;
   }
   out_nl (OUT_STDOUT);
   return;
}
   

/*-------------------------------------------------------------------------
 * Function:    stc_walk1
 *
 * Purpose:     Print memory cast as a structure type.
 *
 * Return:      void
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Dec  6 1996
 *
 * Modifications:
 *              Robb Matzke, 2000-05-23
 *              Removed an extraneous out_pop() call.
 *-------------------------------------------------------------------------
 */
static void
stc_walk1 (obj_t _self, void *mem, int operation, walk_t *wdata) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i;
   out_t        *f;

   switch (operation) {
   case WALK_PRINT:
      f = wdata->f;

      /*
       * Walk each of the structure members.
       */
      if (self->walk1) {
         (self->walk1)(_self, mem, operation, wdata);
      } else {
         if (self->name && self->name[0]) out_printf (f, "%s: ", self->name);
         out_printf (f, "struct");
         out_indent (f);
         for (i=0; i<self->ncomps && !out_brokenpipe(f); i++) {
            out_push (f, self->compname[i]);
            out_nl (f);
            obj_walk1 (self->sub[i], (char*)mem+self->offset[i], operation,
                       wdata);
            out_pop (f);
         }
         out_undent (wdata->f);
      }
      break;

   default:
      abort();
   }
}


/*-------------------------------------------------------------------------
 * Function:    stc_walk2_DBdirectory
 *
 * Purpose:     Diff's two directories.
 *
 * Return:      Success:
 *                 0: A and B are identical, nothing has been printed.
 *                 1: A and B are paritally the same, a summary of the
 *                    differences has been printed.
 *                 2: A and B are totally different, nothing has been
 *                    printed yet.
 *
 *              Failure:        -1
 *
 * Programmer:  Robb Matzke
 *              matzke@viper.llnl.gov
 *              Jul 25 1997
 *
 * Modifications:
 *              Robb Matzke, 2000-05-25
 *              If file A and file B are two different files then change
 *              current working directories in each and call file_diff(),
 *              which is probably faster.
 *-------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
stc_walk2_DBdirectory (obj_t _a, void *a_mem, obj_t _b, void *b_mem,
                       walk_t *wdata)
{
    obj_t       a_file = sdo_file (wdata->a_sdo);
    obj_t       b_file = sdo_file (wdata->b_sdo);
    DBdirectory *a_dir = (DBdirectory *)a_mem;
    DBdirectory *b_dir = (DBdirectory *)b_mem;
    toc_t       *a_toc = a_dir->toc;
    toc_t       *b_toc = b_dir->toc;
    obj_t       a_subdir=NIL;
    obj_t       b_subdir=NIL;
    obj_t       aobj=NIL, bobj=NIL;
    int         an = a_dir->nsyms;
    int         bn = b_dir->nsyms;
    char        cwd[1024], buf[1024];
    obj_t       sym=NIL;
   
    int         i, j, ndiff=0;
    int         status, nonlya=0, nonlyb=0;
    out_t       *f = OUT_STDOUT;

    while (file_file(a_file)!=file_file(b_file)) {
        /* The file_diff() function is probably faster because it uses the
         * current working directory (significantly faster for the
         * Silo/PDB driver).  However, we can only use it if we're
         * differencing in two different files (because it needs to keep
         * track of two different cwds). */
        char a_cwd[1024], b_cwd[1024];
        int retval;

        /* Save current directories */
        if (DBGetDir(file_file(a_file), a_cwd)<0) break;
        if (DBGetDir(file_file(b_file), b_cwd)<0) break;

        /* Set cwds */
        if (DBSetDir(file_file(a_file), obj_name(wdata->a_sdo))<0) break;
        if (DBSetDir(file_file(b_file), obj_name(wdata->b_sdo))<0) {
            status = DBSetDir(file_file(a_file), a_cwd);
            assert(status>=0);
            break;
        }

        /* Diff the files */
        retval = obj_diff(a_file, b_file);

        /* Reset cwds */
        status = DBSetDir(file_file(a_file), a_cwd);
        assert(status>=0);
        status = DBSetDir(file_file(b_file), b_cwd);
        assert(status>=0);

        /* Free resources */
        obj_dest(a_file);
        obj_dest(b_file);

        return retval;
    }
    

    if (Verbosity>=1) {
        out_info ("Differencing directories %s%s and %s%s",
                  obj_name (a_file), obj_name(wdata->a_sdo),
                  obj_name (b_file), obj_name(wdata->b_sdo));
    }

    /* Get the table of contents for each file. */
    assert (a_file && b_file);
    assert ((0==an || a_toc) && (0==bn || b_toc));

    for (i=j=ndiff=0; i<an || j<bn; i++,j++) {
        out_section(f);
        if (out_brokenpipe(f)) break;

        /* List the names of objects that appear only in A. */        
        nonlya = 0;
        if (!DiffOpt.ignore_dels) {
            while (i<an && (j>=bn || strcmp(a_toc[i].name, b_toc[j].name)<0)) {
                out_section(f);
                switch (DiffOpt.report) {
                case DIFF_REP_ALL:
                case DIFF_REP_BRIEF:
                    out_push (f, a_toc[i].name);
                    out_puts (f, "appears only in file A");
                    out_nl (f);
                    out_pop (f);
                    break;
                case DIFF_REP_SUMMARY:
                    return 2;
                }
                nonlya++;
                ndiff++;
                i++;
            }
        }

        /* List the names of objects that appear only in B. */        
        nonlyb = 0;
        if (!DiffOpt.ignore_adds) {
            while (j<bn && (i>=an || strcmp(b_toc[j].name, a_toc[i].name)<0)) {
                out_section(f);
                switch (DiffOpt.report) {
                case DIFF_REP_ALL:
                case DIFF_REP_BRIEF:
                    out_push (f, b_toc[j].name);
                    out_puts (f, "appears only in file B");
                    out_nl (f);
                    out_pop (f);
                    break;
                case DIFF_REP_SUMMARY:
                    return 2;
                }
                nonlyb++;
                ndiff++;
                j++;
            }
        }

        if (i<an && BROWSER_DB_DIR==a_toc[i].type &&
            j<bn && BROWSER_DB_DIR==b_toc[j].type) {
            /* Diff two subdirectories. */
            out_section(f);
            assert (0==strcmp (a_toc[i].name, b_toc[j].name));

            sprintf (buf, "%s/%s", obj_name(wdata->a_sdo), a_toc[i].name);
            sym = obj_new (C_SYM, buf);
            a_subdir = obj_deref (a_file, 1, &sym);
            sym = obj_dest (sym);

            sprintf (buf, "%s/%s", obj_name(wdata->b_sdo), b_toc[j].name);
            sym = obj_new (C_SYM, buf);
            b_subdir = obj_deref (b_file, 1, &sym);
            sym = obj_dest (sym);

            out_push (f, a_toc[i].name);
            obj_diff (a_subdir, b_subdir);
            out_pop(f);
            a_subdir = obj_dest (a_subdir);
            b_subdir = obj_dest (b_subdir);

        } else if (i<an && j<bn) {
            /* Diff two objects. */
            out_section(f);
            if (Verbosity>=1) {
                assert (0==strcmp (a_toc[i].name, b_toc[j].name));
                strcpy (cwd, "Differencing: ");
                DBGetDir (file_file(a_file), cwd+14);
                if (strcmp(cwd+14,"/")) strcat (cwd, "/");
                strcat (cwd, a_toc[i].name);
                out_progress (cwd);
            }

            sprintf (buf, "%s/%s", obj_name(wdata->a_sdo), a_toc[i].name);
            sym = obj_new (C_SYM, buf);
            aobj = obj_deref (a_file, 1, &sym);
            sym = obj_dest (sym);

            sprintf (buf, "%s/%s", obj_name(wdata->b_sdo), b_toc[j].name);
            sym = obj_new (C_SYM, buf);
            bobj = obj_deref (b_file, 1, &sym);
            sym = obj_dest (sym);
         
            assert(aobj && bobj);
            out_push (f, a_toc[i].name);
            status = obj_diff (aobj, bobj);
            if (status) ndiff++;

            switch (DiffOpt.report) {
            case DIFF_REP_ALL:
                if (2==status) {
                    out_line (f, "***************");
                    obj_print (aobj, f);
                    out_line (f, "---------------");
                    obj_print (bobj, f);
                    out_line (f, "***************");
                }
                break;
            case DIFF_REP_BRIEF:
                if (2==status) {
                    out_puts(f, "different value(s)");
                    out_nl(f);
                }
                break;
            case DIFF_REP_SUMMARY:
                if (status) {
                    out_pop(f);
                    aobj = obj_dest(aobj);
                    bobj = obj_dest(bobj);
                    out_progress(NULL);
                    obj_dest(a_file);
                    obj_dest(b_file);
                    return 2;
                }
            }
            out_pop (f);
            aobj = obj_dest (aobj);
            bobj = obj_dest (bobj);
        }
    }

    out_progress (NULL);
    obj_dest (a_file);
    obj_dest (b_file);
    return ndiff ? 1 : 0;
}


/*-------------------------------------------------------------------------
 * Function:    stc_walk2
 *
 * Purpose:     Determines if two structured objects are the same.
 *
 * Return:      Success:
 *                 0: A and B are identical, nothing has been printed.
 *                 1: A and B are partially the same, a summary of the
 *                    differences has been printed.
 *                 2: A and B are totally different, nothing has been
 *                    printed yet.
 *
 *              Failure:        -1
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Jan 21 1997
 *
 * Modifications:
 *              Robb Matzke, 2000-05-23
 *              The output depends on the DiffOpt global variable. In fact,
 *              with certain settings we can even short-circuit some of
 *              the work.
 *-------------------------------------------------------------------------
 */
static int
stc_walk2 (obj_t _a, void *a_mem, obj_t _b, void *b_mem, walk_t *wdata) {

   obj_stc_t    *a = MYCLASS(_a);
   obj_stc_t    *b = MYCLASS(_b);
   int          i, j, status, na, nb;
   out_t        *f = wdata->f;
   int          *a_diff, *b_diff;
   int          nsame=0, ndiffer=0, nonlya=0, nonlyb=0;

   /*
    * Clear the difference indicators by setting them each to -999.
    * When A differs from B, a_diff will contain the index of B and
    * b_diff will contain the index of A.
    */
   a_diff = calloc (a->ncomps, sizeof(int));
   for (i=0; i<a->ncomps; i++) a_diff[i] = -999;
   b_diff = calloc (b->ncomps, sizeof(int));
   for (i=0; i<b->ncomps; i++) b_diff[i] = -999;

   /*
    * If both types have localized their walk2 functions, then call that
    * function instead.
    */
   if (a->walk2 && a->walk2==b->walk2) {
      return (a->walk2)(_a, a_mem, _b, b_mem, wdata);
   }

   /*
    * Figure out which fields appear only in A (a_diff[i]==b->ncomps),
    * which appear only in B (b_diff[i]==a->ncomps), which appear in both
    * but differ (a_diff[i]=j, b_diff[j]=i, i>=0, j>=0), and which are
    * the same in both.
    */
   for (i=0; i<a->ncomps; i++) {
      if (out_brokenpipe(f)) return -1;
      for (j=0; j<b->ncomps; j++) {
         if (!strcmp(a->compname[i], b->compname[j])) {

            out_push (f, a->compname[i]);
            status = obj_walk2 (a->sub[i], (char*)a_mem+a->offset[i],
                                b->sub[j], (char*)b_mem+b->offset[j], wdata);
            out_pop (f);

            switch (DiffOpt.report) {
            case DIFF_REP_ALL:
            case DIFF_REP_BRIEF:
                if (status<0 || 1==status) {
                    ndiffer++;
                } else if (0==status) {
                    nsame++;
                } else {
                    ndiffer++;
                    a_diff[i] = j;
                    b_diff[j] = i;
                }
                break;
            case DIFF_REP_SUMMARY:
                return 1;
            }
            break;
         }
      }
      if (j>=b->ncomps) {
          switch (DiffOpt.report) {
          case DIFF_REP_ALL:
          case DIFF_REP_BRIEF:
              if (!DiffOpt.ignore_dels) {
                  ndiffer++;
                  nonlya++;
                  a_diff[i] = b->ncomps;
              } else {
                  nsame++;
              }
              break;
          case DIFF_REP_SUMMARY:
              if (!DiffOpt.ignore_dels) return 2;
              nsame++;
              break;
          }
      }
   }
   for (j=0; j<b->ncomps; j++) {
      for (i=0; i<a->ncomps; i++) {
         if (!strcmp(a->compname[i], b->compname[j])) break;
      }
      if (i>=a->ncomps) {
          switch (DiffOpt.report) {
          case DIFF_REP_ALL:
          case DIFF_REP_BRIEF:
              if (!DiffOpt.ignore_adds) {
                  ndiffer++;
                  nonlyb++;
                  b_diff[j] = a->ncomps;
              } else {
                  nsame++;
              }
              break;
          case DIFF_REP_SUMMARY:
              if (!DiffOpt.ignore_adds) return 2;
              nsame++;
              break;
          }
      }
   }

   /* Print the A side of things */
   for (i=na=0; i<a->ncomps; i++) {
       if (out_brokenpipe(f)) return -1;
       if (a_diff[i]==b->ncomps && !DiffOpt.ignore_dels) {
           switch (DiffOpt.report) {
           case DIFF_REP_ALL:
               out_push(f, a->compname[i]);
               if (DiffOpt.two_column) {
                   obj_walk1(a->sub[i], (char*)a_mem+a->offset[i],
                             WALK_PRINT, wdata);
                   out_column(f, OUT_COL2, DIFF_SEPARATOR);
                   out_puts(f, DIFF_NOTAPP);
               } else {
                   if (0==na) out_line(f, "***************");
                   obj_walk1(a->sub[i], (char*)a_mem+a->offset[i],
                             WALK_PRINT, wdata);
               }
               out_pop(f);
               out_nl(f);
               break;
           case DIFF_REP_BRIEF:
               out_push(f, a->compname[i]);
               out_puts(f, "appears only in file A");
               out_nl(f);
               out_pop(f);
               break;
           case DIFF_REP_SUMMARY:
               return 2;
           }
           na++;
       } else if (a_diff[i]>=0 && a_diff[i]<b->ncomps) {
           switch (DiffOpt.report) {
           case DIFF_REP_ALL:
               out_push(f, a->compname[i]);
               if (DiffOpt.two_column) {
                   obj_walk1(a->sub[i], (char*)a_mem+a->offset[i],
                             WALK_PRINT, wdata);
                   out_column(f, OUT_COL2, DIFF_SEPARATOR);
                   obj_walk1(b->sub[a_diff[i]],
                             (char*)b_mem+b->offset[a_diff[i]],
                             WALK_PRINT, wdata);
               } else {
                   if (0==na) out_line(f, "***************");
                   obj_walk1(a->sub[i], (char*)a_mem+a->offset[i],
                             WALK_PRINT, wdata);
               }
               out_nl(f);
               out_pop(f);
               break;
           case DIFF_REP_BRIEF:
               out_push(f, a->compname[i]);
               out_puts(f, "different value(s)");
               out_nl(f);
               out_pop(f);
               break;
           case DIFF_REP_SUMMARY:
               return 2;
           }
           na++;
       }
   }

   /* Print the B side of things */
   for (i=nb=0; i<b->ncomps; i++) {
       if (out_brokenpipe(f)) return -1;
       if (b_diff[i]==a->ncomps && !DiffOpt.ignore_adds) {
           switch (DiffOpt.report) {
           case DIFF_REP_ALL:
               out_push(f, b->compname[i]);
               if (DiffOpt.two_column) {
                   out_puts(f, DIFF_NOTAPP);
                   out_column(f, OUT_COL2, DIFF_SEPARATOR);
                   obj_walk1(b->sub[i], (char*)b_mem+b->offset[i],
                             WALK_PRINT, wdata);
               } else {
                   if (0==nb) {
                       if (0==na) out_line(f, "***************");
                       out_line(f, "---------------");
                   }
                   obj_walk1(b->sub[i], (char*)b_mem+b->offset[i],
                             WALK_PRINT, wdata);
               }
               out_nl(f);
               out_pop(f);
               break;
           case DIFF_REP_BRIEF:
               out_push(f, b->compname[i]);
               out_puts(f, "appears only in file B");
               out_nl(f);
               out_pop(f);
               break;
           case DIFF_REP_SUMMARY:
               return 2;
           }
           nb++;
       } else if (b_diff[i]>=0 && b_diff[i]<a->ncomps) {
           switch (DiffOpt.report) {
           case DIFF_REP_ALL:
               if (DiffOpt.two_column) {
                   /* already printed */
               } else {
                   if (0==nb) {
                       if (0==na) out_line(f, "***************");
                       out_line(f, "---------------");
                   }
                   out_push (f, a->compname[i]);
                   obj_walk1(b->sub[i], (char*)b_mem+b->offset[i],
                             WALK_PRINT, wdata);
                   out_nl(f);
                   out_pop (f);
               }
               break;
           case DIFF_REP_BRIEF:
               break; /*already printed*/
           case DIFF_REP_SUMMARY:
               return 2;
           }
           nb++;
       }
   }
   if ((na || nb) && DIFF_REP_ALL==DiffOpt.report && !DiffOpt.two_column) {
       out_line (f, "***************");
   }

   /* Return */
   if (ndiffer) {
       if (DiffOpt.two_column) return 1;
       if (na || nb) return 1;
       return 2;
   }
   return 0;
}


/*-------------------------------------------------------------------------
 * Function:    stc_deref
 *
 * Purpose:     Given a structure type, return the subtype of the component
 *              with name COMP.  Call stc_offset with SELF and COMP to get
 *              the byte offset for the specified component from the
 *              beginning of the structure.
 *
 * Return:      Success:        Ptr to a copy of the subtype.
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Dec 10 1996
 *
 * Modifications:
 *
 *      Robb Matzke, 4 Feb 1997
 *      The prototype changed but the functionality remains the same.
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_deref (obj_t _self, int argc, obj_t argv[]) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i;
   char         *compname;

   if (1!=argc) {
      out_errorn ("stc_deref: wrong number of arguments");
      return NIL;
   }
   if (argv[0] && C_NUM==argv[0]->pub.cls) {
      out_errorn ("stc_deref: an array index cannot be applied to a "
                  "structure");
      return NIL;
   }

   compname = obj_name (argv[0]);
   assert (self && compname);

   for (i=0; i<self->ncomps; i++) {
      if (!strcmp(self->compname[i], compname)) {
         return obj_copy (self->sub[i], SHALLOW);
      }
   }

   out_errorn ("stc_deref: structure component doesn't exist: %s",
               compname);
   return NIL;
}


/*-------------------------------------------------------------------------
 * Function:    stc_name
 *
 * Purpose:     Returns a pointer to the structure name.
 *
 * Return:      Success:        Ptr to structure name
 *
 *              Failure:        NULL
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Dec 17 1996
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static char *
stc_name (obj_t _self) {

   obj_stc_t    *self = MYCLASS(_self);

   if (self->name && self->name[0]) return self->name;
   return NULL;
}


/*-------------------------------------------------------------------------
 * Function:    stc_bind
 *
 * Purpose:     Binds array dimensions to numeric values.
 *
 * Return:      Success:        SELF
 *
 *              Failure:        NIL
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Jan 13 1997
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static obj_t
stc_bind (obj_t _self, void *mem) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i, nerrors=0;
   obj_t        saved=NIL, sdo=NIL;

   if (!mem) return _self;

   /*
    * Save current value of variable `self' and set `self' to point
    * to an SDO with the MEM and type SELF.
    */
   sdo = obj_new (C_SDO, NIL, NULL, mem, self, mem, self, NULL, NULL, NULL);
   saved = sym_self_set (sdo);
   sdo=NIL;
   
   /*
    * Bind each of the component types.
    */
   for (i=0; i<self->ncomps; i++) {
      if (NIL==obj_bind(self->sub[i], (char*)mem+self->offset[i])) {
         nerrors++;
      }
   }

   /*
    * Restore the previous value of variable `self'.
    */
   obj_dest (sym_self_set (saved));
   saved = NIL;

   return nerrors ? NIL : _self;
}


/*-------------------------------------------------------------------------
 * Function:    stc_sort
 *
 * Purpose:     Destructively sorts the fields of a structure.
 *
 * Return:      void
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Feb  5 1997
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
void
stc_sort (obj_t _self, int start_at) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i, j, mini, tmp_off;
   char         *tmp_str=NULL;
   obj_t        tmp_sub=NIL;

   for (i=start_at; i<self->ncomps-1; i++) {
      mini = i;
      for (j=i+1; j<self->ncomps; j++) {
         if (strcmp (self->compname[j], self->compname[mini])<0) {
            mini = j;
         }
      }
      if (mini!=i) {
         tmp_str = self->compname[i];
         self->compname[i] = self->compname[mini];
         self->compname[mini] = tmp_str;

         tmp_sub = self->sub[i];
         self->sub[i] = self->sub[mini];
         self->sub[mini] = tmp_sub;

         tmp_off = self->offset[i];
         self->offset[i] = self->offset[mini];
         self->offset[mini] = tmp_off;
      }
   }
}


/*-------------------------------------------------------------------------
 * Function:    stc_offset
 *
 * Purpose:     Given a structure and a component name, return the byte
 *              offset of the subtype from the beginning of the structure.
 *
 * Return:      Success:        Byte offset to specified structure member.
 *
 *              Failure:        -1
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Dec 10 1996
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
int
stc_offset (obj_t _self, obj_t comp) {

   obj_stc_t    *self = MYCLASS(_self);
   int          i;
   char         *compname = obj_name (comp);

   for (i=0; i<self->ncomps; i++) {
      if (!strcmp(self->compname[i], compname)) {
         return self->offset[i];
      }
   }

   out_errorn ("stc_offset: no such structure component: %s", compname);
   return -1;
}


/*-------------------------------------------------------------------------
 * Function:    stc_silo_types
 *
 * Purpose:     Initializes the silo data types.
 *
 * Return:      void
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Jan 13 1997
 *
 * Modifications:
 *
 *  Robb Matzke, 18 Feb 1997
 *  Added limits to arrays based on `ndims'.
 *
 *  Robb Matzke, 25 Mar 1997
 *  Changed the `min_extents', `max_extents', and `align' fields to use
 *  the value stored in the datatype field.  See related comments in the
 *  silo.h file.
 *
 *  Robb Matzke, 29 Jul 1997
 *  Array special handling flag `SH1' now takes an optional argument
 *  which, if it evaluates to anything other than DB_COLLINEAR, the
 *  special handling is ignored.  If it isn't ignored, then the specified
 *  array is really a 1-d array where the array size is one of the numbers
 *  specified depending on the current index of the enclosing array.
 *
 *  Robb Matzke, 2 Sep 1997
 *  The `min_extents', `max_extents', and `align' fields are always
 *  DB_FLOAT.
 *
 *  Sean Ahern, Tue Mar 31 17:43:03 PST 1998
 *  Changed the `min_extents', and `max_extents' fields in UCD meshes to use
 *  the value stored in the datatype field.
 *
 *  Jeremy Meredith, Sept 21 1998
 *  Added multimatspecies object.
 *
 *  Sean Ahern, Tue Oct 20 14:35:13 PDT 1998
 *  Changed the `min_extents' and `max_extents' fields in Quadmeshes to use 
 *  the value stored in the datatype field.
 *
 *  Jeremy Meredith, Wed Jul  7 12:15:31 PDT 1999
 *  I removed the origin from the species object.
 *
 *  Robb Matzke, Thu Jul 15 12:40:37 EDT 1999
 *  Added `group_no' to DBpointmesh, DBquadmesh, and DBucdmesh.
 *  
 *  Robb Matzke, Thu Jul 15 12:40:37 EDT 1999
 *  Added `ngroups', `blockorigin', and `grouporigin' to DBmultimesh,
 *  DBmultivar, DBmultimat, and DBmultimatspecies.
 *  
 *  Robb Matzke, Thu Jul 15 12:40:37 EDT 1999
 *  Added `min_index', `max_index', `zoneno', and `gzoneno' to the DBzonelist
 *  type.
 *  
 *  Robb Matzke, Thu Jul 15 12:40:37 EDT 1999
 *  Added `base_index[]' to the DBquadmesh type.
 *  
 *  Robb Matzke, Thu Jul 15 12:40:37 EDT 1999
 *  Added `gnodeno' and `nodeno' properties to the DBucdmesh type.
 *  
 *  Thomas R. Treadway, Fri Jul  7 12:44:38 PDT 2006
 *  Added support for DBOPT_REFERENCE in Curves
 *  
 *-------------------------------------------------------------------------
 */
void
stc_silo_types (void) {

   obj_t        tmp=NIL;

   STRUCT (DBobject) {
      COMP (name,               "primitive 'string'");
      COMP (type,               "primitive 'string'");
      COMP (ncomponents,        "primitive 'int'");
      COMP (maxcomponents,      "primitive 'int'");
      COMP (comp_names,
            "pointer (array 'self.ncomponents' (primitive 'string'))");
      COMP (pdb_names,
            "pointer (array 'self.ncomponents' (primitive 'string'))");
   } ESTRUCT;

   STRUCT (DBcurve) {
      COMP (id,                 "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (origin,             "primitive 'int'");
      COMP (title,              "primitive 'string'");
      COMP (xvarname,           "primitive 'string'");
      COMP (yvarname,           "primitive 'string'");
      COMP (xlabel,             "primitive 'string'");
      COMP (ylabel,             "primitive 'string'");
      COMP (xunits,             "primitive 'string'");
      COMP (yunits,             "primitive 'string'");
      COMP (npts,               "primitive 'int'");
      COMP (reference,          "primitive 'string'");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (x,
            "pointer (array 'self.npts' (primitive 'self.datatype'))");
      COMP (y,
            "pointer (array 'self.npts' (primitive 'self.datatype'))");
   } ESTRUCT;

   STRUCT (DBdefvars) {
      COMP (ndefs,              "primitive 'int'");
      COMP (names,
            "pointer (array 'self.ndefs' (primitive 'string'))");

      tmp = obj_new (C_PRIM, "int");
      prim_set_io_assoc (tmp, PA_DEFVARTYPE);
      tmp = obj_new (C_PTR, obj_new (C_ARY, "self.ndefs", tmp));
      COMP3 (types, "types", tmp);

      COMP (defns,
            "pointer (array 'self.ndefs' (primitive 'string'))");
      COMP (guihides,
            "pointer (array 'self.ndefs' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBmultimesh) {
      COMP (id,                 "primitive 'int'");
      COMP (nblocks,            "primitive 'int'");
      COMP (ngroups,            "primitive 'int'");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (meshids,
            "pointer (array 'self.nblocks' (primitive 'int'))");
      COMP (meshnames,
            "pointer (array 'self.nblocks' (primitive 'string'))");

      tmp = obj_new (C_PRIM, "int");
      prim_set_io_assoc (tmp, PA_OBJTYPE);
      tmp = obj_new (C_PTR, obj_new (C_ARY, "self.nblocks", tmp));
      COMP3 (meshtypes, "meshtypes", tmp);

      COMP (dirids,
            "pointer (array 'self.nblocks' (primitive 'int'))");
      COMP (blockorigin,        "primitive 'int'");
      COMP (grouporigin,        "primitive 'int'");
      COMP (extentssize,        "primitive 'int'");
      COMP (extents,
            "pointer (array 'self.nblocks,self.extentssize' (primitive 'double'))");
      COMP (zonecounts,
            "pointer (array 'self.nblocks' (primitive 'int'))");
      COMP (has_external_zones,
            "pointer (array 'self.nblocks' (primitive 'int'))");
      COMP (lgroupings,        "primitive 'int'");
      COMP (groupings,
            "pointer (array 'self.lgroupings' (primitive 'int'))");
      COMP (groupnames,
            "pointer (array 'self.lgroupings' (primitive 'string'))");
   } ESTRUCT;

   STRUCT (DBmultimeshadj) {
      COMP (nblocks,            "primitive 'int'");
      COMP (blockorigin,        "primitive 'int'");
      COMP (lneighbors,         "primitive 'int'");
      COMP (totlnodelists,      "primitive 'int'");
      COMP (totlzonelists,      "primitive 'int'");

      tmp = obj_new (C_PRIM, "int");
      prim_set_io_assoc (tmp, PA_OBJTYPE);
      tmp = obj_new (C_PTR, obj_new (C_ARY, "self.nblocks", tmp));
      COMP3 (meshtypes, "meshtypes", tmp);

      COMP (nneighbors,
            "pointer (array 'self.nblocks' (primitive 'int'))");
      COMP (neighbors,
            "pointer (array 'self.lneighbors' (primitive 'int'))");
      COMP (back,
            "pointer (array 'self.lneighbors' (primitive 'int'))");
      COMP (lnodelists,
            "pointer (array 'self.lneighbors' (primitive 'int'))");
      COMP (lzonelists,
            "pointer (array 'self.lneighbors' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBmultivar) {
      COMP (id,                 "primitive 'int'");
      COMP (nvars,              "primitive 'int'");
      COMP (ngroups,            "primitive 'int'");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (varnames,
            "pointer (array 'self.nvars' (primitive 'string'))");

      tmp = obj_new (C_PRIM, "int");
      prim_set_io_assoc (tmp, PA_OBJTYPE);
      tmp = obj_new (C_PTR, obj_new (C_ARY, "self.nvars", tmp));
      COMP3 (vartypes, "vartypes", tmp);
      tmp = NIL;

      COMP (blockorigin,        "primitive 'int'");
      COMP (grouporigin,        "primitive 'int'");
      COMP (extentssize,        "primitive 'int'");
      COMP (extents,
            "pointer (array 'self.nvars,self.extentssize' (primitive 'double'))");
   } ESTRUCT;


   STRUCT (browser_DBmultimat) {
      COMP (id,                 "primitive 'int'");
      COMP (nmats,              "primitive 'int'");
      COMP (ngroups,            "primitive 'int'");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (matnames,
            "pointer (array 'self.nmats' (primitive 'string'))");
      COMP (blockorigin,        "primitive 'int'");
      COMP (grouporigin,        "primitive 'int'");
      COMP (mixlens,
            "pointer (array 'self.nmats' (primitive 'int'))");
      COMP (matcounts,
            "pointer (array 'self.nmats' (primitive 'int'))");
      COMP (matlists,
            "pointer (array 'self.nmats' (primitive 'int'))");
      COMP (nmatnos,            "primitive 'int'");
      COMP (matnos,
            "pointer (array 'self.nmatnos' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (browser_DBmultimatspecies) {
      COMP (id,                 "primitive 'int'");
      COMP (nspec,              "primitive 'int'");
      COMP (ngroups,            "primitive 'int'");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (specnames,
            "pointer (array 'self.nspec' (primitive 'string'))");
      COMP (blockorigin,        "primitive 'int'");
      COMP (grouporigin,        "primitive 'int'");
      COMP (nmat,               "primitive 'int'");
      COMP (nmatspec,
            "pointer (array 'self.nmat' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBquadmesh) {
      COMP (id,                 "primitive 'int'");
      COMP (block_no,           "primitive 'int'");
      COMP (group_no,           "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (coord_sys,          "primitive 'int'");
      IOASSOC (PA_COORDSYS);
      COMP (major_order,        "primitive 'int'");
      IOASSOC (PA_ORDER);
      COMP (stride,             "array 3 (primitive 'int')");
      COMP (coordtype,          "primitive 'int'");
      IOASSOC (PA_COORDTYPE);
      COMP (facetype,           "primitive 'int'");
      IOASSOC (PA_FACETYPE);
      COMP (planar,             "primitive 'int'");
      IOASSOC (PA_PLANAR);
      COMP (ndims,              "primitive 'int'");
      COMP (nspace,             "primitive 'int'");
      COMP (nnodes,             "primitive 'int'");
      COMP (dims,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (min_index,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (max_index,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (origin,             "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (min_extents,
            "array 'SH3 3, self.ndims' (primitive 'self.datatype')");
      COMP (max_extents,
            "array 'SH3 3, self.ndims' (primitive 'self.datatype')");
      COMP (labels,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (units,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (coords,
            "array 'SH3 3, self.ndims' "
            "(pointer (array 'SH1 self.coordtype, self.dims' "
            "(primitive 'self.datatype')))");
      COMP (base_index,         "array 3 (primitive 'int')");
   } ESTRUCT;

   STRUCT (DBquadvar) {
      COMP (id,                 "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (meshname,           "primitive 'string'");
      COMP (units,              "primitive 'string'");
      COMP (label,              "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (meshid,             "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (nels,               "primitive 'int'");
      COMP (nvals,              "primitive 'int'");
      COMP (ndims,              "primitive 'int'");
      COMP (dims,               "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (major_order,        "primitive 'int'");
      IOASSOC (PA_ORDER);
      COMP (stride,             "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (min_index,          "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (max_index,          "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (origin,             "primitive 'int'");
      COMP (align,
            "array 'SH3 3, self.ndims' (primitive 'float')");
      COMP (mixlen,             "primitive 'int'");
      COMP (use_specmf,         "primitive 'int'");
      IOASSOC (PA_ONOFF);
      COMP (ascii_labels,       "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (mixvals,
            "pointer (array 'self.nvals' (pointer (array 'self.mixlen' "
            "(primitive 'self.datatype'))))");
      COMP (vals,
            "pointer (array 'self.nvals' (pointer (array 'self.nels' "
            "(primitive 'self.datatype'))))");
   } ESTRUCT;

   STRUCT (DBzonelist) {
      COMP (ndims,              "primitive 'int'");
      COMP (nzones,             "primitive 'int'");
      COMP (nshapes,            "primitive 'int'");
      COMP (shapecnt,
            "pointer (array 'self.nshapes' (primitive 'int'))");
      COMP (shapesize,
            "pointer (array 'self.nshapes' (primitive 'int'))");
      COMP (shapetype,
            "pointer (array 'self.nshapes' (primitive 'int'))");
      COMP (lnodelist,          "primitive 'int'");
      COMP (nodelist,
            "pointer (array 'self.lnodelist' (primitive 'int'))");
      COMP (origin,             "primitive 'int'");
      COMP (min_index,          "primitive 'int'");
      COMP (max_index,          "primitive 'int'");
      COMP (zoneno,
            "pointer (array 'self.nzones' (primitive 'int'))");
      COMP (gzoneno,
            "pointer (array 'self.nzones' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBphzonelist) {
      COMP (nfaces,             "primitive 'int'");
      COMP (nodecnt,
            "pointer (array 'self.nfaces' (primitive 'int'))");
      COMP (lnodelist,          "primitive 'int'");
      COMP (nodelist,
            "pointer (array 'self.lnodelist' (primitive 'int'))");
      COMP (extface,
            "pointer (array 'self.nfaces' (primitive 'int8'))");
      COMP (nzones,             "primitive 'int'");
      COMP (facecnt,
            "pointer (array 'self.nzones' (primitive 'int'))");
      COMP (lfacelist,          "primitive 'int'");
      COMP (facelist,
            "pointer (array 'self.lfacelist' (primitive 'int'))");
      COMP (origin,             "primitive 'int'");
      COMP (lo_offset,          "primitive 'int'");
      COMP (hi_offset,          "primitive 'int'");
      COMP (zoneno,
            "pointer (array 'self.nzones' (primitive 'int'))");
      COMP (gzoneno,
            "pointer (array 'self.nzones' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBcsgzonelist) {
      COMP (nregs,             "primitive 'int'");
      COMP (origin,            "primitive 'int'");

      tmp = obj_new (C_PRIM, "int");
      prim_set_io_assoc (tmp, PA_REGIONOP);
      tmp = obj_new (C_PTR, obj_new (C_ARY, "self.nregs", tmp));
      COMP3 (typeflags, "typeflags", tmp);

      COMP (leftids,
            "pointer (array 'self.nregs' (primitive 'int'))");
      COMP (rightids,
            "pointer (array 'self.nregs' (primitive 'int'))");
      COMP (lxform,            "primitive 'int'");
      COMP (datatype,          "primitive 'int'");
      COMP (xform,
            "pointer (array 'self.lxform' (primitive 'self.datatype'))");
      COMP (nzones,            "primitive 'int'");
      COMP (zonelist,
            "pointer (array 'self.nzones' (primitive 'int'))");
      COMP (min_index,         "primitive 'int'");
      COMP (max_index,         "primitive 'int'");
      COMP (regnames,
            "pointer (array 'self.nregs' (primitive 'string'))");
      COMP (zonenames,
            "pointer (array 'self.nzones' (primitive 'string'))");
   } ESTRUCT;

   STRUCT (DBfacelist) {
      COMP (ndims,              "primitive 'int'");
      COMP (nfaces,             "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (lnodelist,          "primitive 'int'");
      COMP (nodelist,
            "pointer (array 'self.lnodelist' (primitive 'int'))");
      COMP (nshapes,            "primitive 'int'");
      COMP (shapecnt,
            "pointer (array 'self.nshapes' (primitive 'int'))");
      COMP (shapesize,
            "pointer (array 'self.nshapes' (primitive 'int'))");
      COMP (ntypes,             "primitive 'int'");
      COMP (typelist,
            "pointer (array 'self.ntypes' (primitive 'int'))");
      COMP (types,
            "pointer (array 'self.nfaces' (primitive 'int'))");
      COMP (nodeno,
            "pointer (array 'self.lnodelist' (primitive 'int'))");
      COMP (zoneno,
            "pointer (array 'self.nfaces' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBedgelist) {
      COMP (ndims,              "primitive 'int'");
      COMP (nedges,             "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (edge_beg,
            "pointer (array 'self.nedges' (primitive 'int'))");
      COMP (edge_end,
            "pointer (array 'self.nedges' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBcsgmesh) {
      COMP (block_no,           "primitive 'int'");
      COMP (group_no,           "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (ndims,              "primitive 'int'");
      COMP (units,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (labels,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (nbounds,            "primitive 'int'");

      tmp = obj_new (C_PRIM, "int");
      prim_set_io_assoc (tmp, PA_BOUNDARYTYPE);
      tmp = obj_new (C_PTR, obj_new (C_ARY, "self.nbounds", tmp));
      COMP3 (typeflags, "typeflags", tmp);

      COMP (bndids,
            "pointer (array 'self.nbounds' (primitive 'int'))");
      COMP (lcoeffs,            "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      COMP (coeffs,
            "pointer (array 'self.lcoeffs' (primitive 'self.datatype'))");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (min_extents,
            "array 'SH3 3, self.ndims' (primitive 'double')");
      COMP (max_extents,
            "array 'SH3 3, self.ndims' (primitive 'double')");
      COMP (origin,             "primitive 'int'");
      COMP (zones,              "pointer 'DBcsgzonelist'");
      COMP (bndnames,
            "pointer (array 'self.nbounds' (primitive 'string'))");
   } ESTRUCT;

   STRUCT (DBcsgvar) {
      COMP (name,               "primitive 'string'");
      COMP (meshname,           "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (units,              "primitive 'string'");
      COMP (label,              "primitive 'string'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (nels,               "primitive 'int'");
      COMP (nvals,              "primitive 'int'");
      COMP (centering,          "primitive 'int'");
      IOASSOC (PA_CENTERING);
      COMP (use_specmf,         "primitive 'int'");
      IOASSOC (PA_ONOFF);
      COMP (ascii_labels,       "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (vals,
            "pointer (array 'self.nvals' (pointer "
            "(array 'self.nels' (primitive 'self.datatype'))))");
   } ESTRUCT;

   STRUCT (DBucdmesh) {
      COMP (id,                 "primitive 'int'");
      COMP (block_no,           "primitive 'int'");
      COMP (group_no,           "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (coord_sys,          "primitive 'int'");
      IOASSOC (PA_COORDSYS);
      COMP (topo_dim,           "primitive 'int'");
      COMP (ndims,              "primitive 'int'");
      COMP (nnodes,             "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (units,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (labels,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (coords,
            "array 'SH3 3, self.ndims' (pointer (array 'self.nnodes' "
            "(primitive 'self.datatype')))");
      COMP (min_extents,
            "array 'SH3 3, self.ndims' (primitive 'self.datatype')");
      COMP (max_extents,
            "array 'SH3 3, self.ndims' (primitive 'self.datatype')");
      COMP (faces,              "pointer 'DBfacelist'");
      COMP (zones,              "pointer 'DBzonelist'");
      COMP (edges,              "pointer 'DBedgelist'");
      COMP (phzones,            "pointer 'DBphzonelist'");
      COMP (gnodeno,
            "pointer (array 'self.nnodes' (primitive 'int'))");
      COMP (nodeno,
            "pointer (array 'self.nnodes' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBucdvar) {
      COMP (id,                 "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (meshname,           "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (units,              "primitive 'string'");
      COMP (label,              "primitive 'string'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (meshid,             "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (nels,               "primitive 'int'");
      COMP (nvals,              "primitive 'int'");
      COMP (ndims,              "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (centering,          "primitive 'int'");
      IOASSOC (PA_CENTERING);
      COMP (mixlen,             "primitive 'int'");
      COMP (use_specmf,         "primitive 'int'");
      IOASSOC (PA_ONOFF);
      COMP (ascii_labels,       "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (vals,
            "pointer (array 'self.nvals' (pointer "
            "(array 'self.nels' (primitive 'self.datatype'))))");
   } ESTRUCT;

   STRUCT (DBpointmesh) {
      COMP (id,                 "primitive 'int'");
      COMP (block_no,           "primitive 'int'");
      COMP (group_no,           "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (title,              "primitive 'string'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (ndims,              "primitive 'int'");
      COMP (nels,               "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (units,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (labels,
            "array 'SH3 3, self.ndims' (primitive 'string')");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (coords,
            "array 'self.ndims' (pointer (array 'self.nels' "
            "(primitive 'self.datatype')))");
      COMP (min_extents,
            "array 'SH3 3, self.ndims' (primitive 'float')");
      COMP (max_extents,
            "array 'SH3 3, self.ndims' (primitive 'float')");
      COMP (gnodeno,
            "pointer (array 'self.nels' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBmeshvar) {
      COMP (id,                 "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (meshname,           "primitive 'string'");
      COMP (units,              "primitive 'string'");
      COMP (label,              "primitive 'string'");
      COMP (cycle,              "primitive 'int'");
      COMP (time,               "primitive 'float'");
      COMP (dtime,              "primitive 'double'");
      COMP (meshid,             "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (nels,               "primitive 'int'");
      COMP (nvals,              "primitive 'int'");
      COMP (nspace,             "primitive 'int'");
      COMP (ndims,              "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (centering,          "primitive 'int'");
      IOASSOC (PA_CENTERING);
      COMP (align,
            "array 'SH3 3, self.ndims' (primitive 'float')");
      COMP (dims,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (major_order,        "primitive 'int'");
      IOASSOC (PA_ORDER);
      COMP (stride,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (min_index,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (max_index,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (ascii_labels,       "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (vals,
            "pointer (array 'self.nvals' (pointer "
            "(array 'self.nels' (primitive 'self.datatype'))))");
   } ESTRUCT;

   STRUCT (DBmaterial) {
      COMP (id,                 "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (meshname,           "primitive 'string'");
      COMP (ndims,              "primitive 'int'");
      COMP (origin,             "primitive 'int'");
      COMP (dims,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (major_order,        "primitive 'int'");
      IOASSOC (PA_ORDER);
      COMP (stride,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (nmat,               "primitive 'int'");
      COMP (matnos,
            "pointer (array 'self.nmat' (primitive 'int'))");
      COMP (matnames,
            "pointer (array 'self.nmat' (primitive 'string'))");
      COMP (matcolors,
            "pointer (array 'self.nmat' (primitive 'string'))");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (matlist,
            "pointer (array 'SH2, self.dims' (primitive 'int'))");
      COMP (mixlen,             "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (mix_vf,
            "pointer (array 'self.mixlen' (primitive 'self.datatype'))");
      COMP (mix_next,
            "pointer (array 'self.mixlen' (primitive 'int'))");
      COMP (mix_mat,
            "pointer (array 'self.mixlen' (primitive 'int'))");
      COMP (mix_zone,
            "pointer (array 'self.mixlen' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBmatspecies) {
      COMP (id,                 "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (matname,            "primitive 'string'");
      COMP (nmat,               "primitive 'int'");
      COMP (nmatspec,
            "pointer (array 'self.nmat' (primitive 'int'))");
      COMP (ndims,              "primitive 'int'");
      COMP (dims,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (major_order,        "primitive 'int'");
      IOASSOC (PA_ORDER);
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (stride,
            "array 'SH3 3, self.ndims' (primitive 'int')");
      COMP (nspecies_mf,        "primitive 'int'");
      COMP (guihide,            "primitive 'int'");
      IOASSOC (PA_BOOLEAN);
      COMP (species_mf,
            "pointer (array 'self.nspecies_mf' (primitive 'self.datatype'))");
      COMP (speclist,
            "pointer (array 'self.dims' (primitive 'int'))");
      COMP (mixlen,             "primitive 'int'");
      COMP (mix_speclist,
            "pointer (array 'self.mixlen' (primitive 'int'))");
   } ESTRUCT;

   STRUCT (DBcompoundarray) {
      COMP (id,                 "primitive 'int'");
      COMP (name,               "primitive 'string'");
      COMP (nelems,             "primitive 'int'");
      COMP (nvalues,            "primitive 'int'");
      COMP (datatype,           "primitive 'int'");
      IOASSOC (PA_DATATYPE);
      COMP (elemnames,
            "pointer (array 'self.nelems' (primitive 'string'))");
      COMP (elemlengths,
            "pointer (array 'self.nelems' (primitive 'int'))");
      COMP (values,
            "pointer (array 'self.nvalues' (primitive 'self.datatype'))");
   } ESTRUCT;

   STRUCT (toc_t) {
      COMP (type,               "primitive 'int'");
      IOASSOC (PA_BR_OBJTYPE);
      COMP (name,               "primitive 'string'");
   } ESTRUCT;

   STRUCT (DBdirectory) {
      WALK1 (stc_walk1_DBdirectory);
      WALK2 (stc_walk2_DBdirectory);
      COMP (nsyms,              "primitive 'int'");
      COMP (entry_ptr,
            "pointer (array 'self.nsyms' (pointer 'toc_t'))");
   } ESTRUCT;

   STRUCT (DBtoc) {
      COMP (ncurve,             "primitive 'int'");
      COMP (curve_names,
            "pointer (array 'self.ncurve' (primitive 'string'))");
      COMP (ndefvars,           "primitive 'int'");
      COMP (defvars_names,
            "pointer (array 'self.ndefvars' (primitive 'string'))");
      COMP (nmultimesh,         "primitive 'int'");
      COMP (multimesh_names,
            "pointer (array 'self.nmultimesh' (primitive 'string'))");
      COMP (nmultimeshadj,       "primitive 'int'");
      COMP (multimeshadj_names,
            "pointer (array 'self.nmultimeshadj' (primitive 'string'))");
      COMP (nmultimat,          "primitive 'int'");
      COMP (multimat_names,
            "pointer (array 'self.nmultimat' (primitive 'string'))");
      COMP (nmultimatspecies,   "primitive 'int'");
      COMP (multimatspecies_names,
            "pointer (array 'self.nmultimatspecies' (primitive 'string'))");
      COMP (ncsgmesh,             "primitive 'int'");
      COMP (csgmesh_names,
            "pointer (array 'self.ncsgmesh' (primitive 'string'))");
      COMP (ncsgvar,              "primitive 'int'");
      COMP (csgvar_names,
            "pointer (array 'self.ncsgvar' (primitive 'string'))");
      COMP (nqmesh,             "primitive 'int'");
      COMP (qmesh_names,
            "pointer (array 'self.nqmesh' (primitive 'string'))");
      COMP (nqvar,              "primitive 'int'");
      COMP (qvar_names,
            "pointer (array 'self.nqvar' (primitive 'string'))");
      COMP (nucdmesh,           "primitive 'int'");
      COMP (ucdmesh_names,
            "pointer (array 'self.nucdmesh' (primitive 'string'))");
      COMP (nucdvar,            "primitive 'int'");
      COMP (ucdvar_names,
            "pointer (array 'self.nucdvar' (primitive 'string'))");
      COMP (nptmesh,            "primitive 'int'");
      COMP (ptmesh_names,
            "pointer (array 'self.nptmesh' (primitive 'string'))");
      COMP (nptvar,             "primitive 'int'");
      COMP (ptvar_names,
            "pointer (array 'self.nptmesh' (primitive 'string'))");
      COMP (nmat,               "primitive 'int'");
      COMP (mat_names,
            "pointer (array 'self.nmat' (primitive 'string'))");
      COMP (nmatspecies,        "primitive 'int'");
      COMP (matspecies_names,
            "pointer (array 'self.nmatspecies' (primitive 'string'))");
      COMP (nvar,               "primitive 'int'");
      COMP (var_names,
            "pointer (array 'self.nvar' (primitive 'string'))");
      COMP (nobj,               "primitive 'int'");
      COMP (obj_names,
            "pointer (array 'self.nobj' (primitive 'string'))");
      COMP (ndir,               "primitive 'int'");
      COMP (dir_names,
            "pointer (array 'self.ndir' (primitive 'string'))");
      COMP (narrays,            "primitive 'int'");
      COMP (array_names,
            "pointer (array 'self.narrays' (primitive 'string'))");
   } ESTRUCT;
}