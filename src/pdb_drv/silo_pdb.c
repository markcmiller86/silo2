/*
 * silo-pdb.c   -- The PDB functions.
 */

#define NEED_SCORE_MM
#include "silo_pdb_private.h"

/* File-wide modifications:
 *      Sean Ahern, Fri Aug  1 13:23:38 PDT 1997
 *      Reformatted tabs to spaces.
 */

static char   *_valstr[10] =
{"value0", "value1", "value2",
 "value3", "value4", "value5",
 "value6", "value7", "value8",
 "value9"};

static char   *_mixvalstr[10] =
{"mixed_value0", "mixed_value1",
 "mixed_value2", "mixed_value3",
 "mixed_value4", "mixed_value5",
 "mixed_value6", "mixed_value7",
 "mixed_value8", "mixed_value9"};

static char   *_ptvalstr[10] =
{"0_data", "1_data", "2_data",
 "3_data", "4_data", "5_data",
 "6_data", "7_data", "8_data",
 "9_data"};

#define CHECK_TYPE(typestring,type,name) \
    if (strcmp(typestring, DBGetObjtypeName(type)) != 0) \
    { \
        char error[256]; \
        sprintf(error,"Requested %s object \"%s\" is not a %s.",\
                typestring, name, DBGetObjtypeName(type)); \
        FREE(typestring); \
        db_perror(error, E_INTERNAL, me); \
    } else { \
        FREE(typestring); \
    }

static PJcomplist *_tcl;

/*-------------------------------------------------------------------------
 * Function:    db_pdb_InitCallbacks
 *
 * Purpose:     Initialize the callbacks in a DBfile structure.
 *
 * Programmer:  matzke@viper
 *              Tue Nov 29 13:26:58 PST 1994
 *
 * Modifications:
 *
 *    Eric Brugger, Thu Feb 16 08:45:00 PST 1995
 *    I added the DBReadVarSlice callback.
 *
 *    Jim Reus, 23 Apr 97
 *    I changed this to prototype form.
 *
 *    Jeremy Meredith, Sept 18 1998
 *    I added multi-material-species
 *
 *    Eric Brugger, Tue Mar 30 10:49:56 PST 1999
 *    I added the DBPutZonelist2 callback.
 *
 *    Robb Matzke, 2000-01-14
 *    I organized callbacks by category for easier comparison with other
 *    drivers.
 *
 *    Brad Whitlock, Thu Jan 20 11:59:11 PDT 2000
 *    I added the DBGetComponentType callback.
 *-------------------------------------------------------------------------*/
PRIVATE void
db_pdb_InitCallbacks ( DBfile *dbfile )
{
    /* Properties of the driver */
    dbfile->pub.pathok = FALSE;         /*driver doesn't handle paths well*/

    /* File operations */
    dbfile->pub.close = db_pdb_close;
    dbfile->pub.module = db_pdb_Filters;

    /* Directory operations */
    dbfile->pub.cd = db_pdb_SetDir;
    dbfile->pub.g_dir = db_pdb_GetDir;
    dbfile->pub.newtoc = db_pdb_NewToc;
    dbfile->pub.cdid = NULL;            /*DBSetDirID() not supported    */
#ifdef PDB_WRITE
    dbfile->pub.mkdir = db_pdb_MkDir;
#endif

    /* Variable inquiries */
    dbfile->pub.exist = db_pdb_InqVarExists;
    dbfile->pub.g_varlen = db_pdb_GetVarLength;
    dbfile->pub.g_varbl = db_pdb_GetVarByteLength;
    dbfile->pub.g_vartype = db_pdb_GetVarType;
    dbfile->pub.g_vardims = db_pdb_GetVarDims;
    dbfile->pub.r_var1 = NULL;          /*DBReadVar1() not supported    */
    dbfile->pub.g_attr = db_pdb_GetAtt;
    dbfile->pub.r_att = db_pdb_ReadAtt;

    /* Variable I/O operations */
    dbfile->pub.g_var = db_pdb_GetVar;
    dbfile->pub.r_var = db_pdb_ReadVar;
    dbfile->pub.r_varslice = db_pdb_ReadVarSlice;
#ifdef PDB_WRITE
    dbfile->pub.write = db_pdb_Write;
    dbfile->pub.writeslice = db_pdb_WriteSlice;
#endif

    /* Low-level object functions */
    dbfile->pub.g_obj = db_pdb_GetObject;
    dbfile->pub.inqvartype = db_pdb_InqVarType;
    dbfile->pub.i_meshtype = db_pdb_InqMeshtype;
    dbfile->pub.i_meshname = db_pdb_InqMeshname;
    dbfile->pub.g_comp = db_pdb_GetComponent;
    dbfile->pub.g_comptyp = db_pdb_GetComponentType;
    dbfile->pub.g_compnames = db_pdb_GetComponentNames;
#ifdef PDB_WRITE
    dbfile->pub.c_obj = db_pdb_WriteObject;      /*overloaded with w_obj*/
    dbfile->pub.w_obj = db_pdb_WriteObject;      /*overloaded with c_obj*/
    dbfile->pub.w_comp = db_pdb_WriteComponent;
#endif

    /* Curve functions */
    dbfile->pub.g_cu = db_pdb_GetCurve;
#ifdef PDB_WRITE
    dbfile->pub.p_cu = db_pdb_PutCurve;
#endif

    /* Defvar functions */
    dbfile->pub.g_defv = db_pdb_GetDefvars;
#ifdef PDB_WRITE
    dbfile->pub.p_defv = db_pdb_PutDefvars;
#endif

    /* Csgmesh functions */
    dbfile->pub.g_csgm = db_pdb_GetCsgmesh;
    dbfile->pub.g_csgv = db_pdb_GetCsgvar;
    dbfile->pub.g_csgzl = db_pdb_GetCSGZonelist;
#ifdef PDB_WRITE
    dbfile->pub.p_csgm = db_pdb_PutCsgmesh;
    dbfile->pub.p_csgv = db_pdb_PutCsgvar;
    dbfile->pub.p_csgzl = db_pdb_PutCSGZonelist;
#endif

    /* Quadmesh functions */
    dbfile->pub.g_qm = db_pdb_GetQuadmesh;
    dbfile->pub.g_qv = db_pdb_GetQuadvar;
#ifdef PDB_WRITE
    dbfile->pub.p_qm = db_pdb_PutQuadmesh;
    dbfile->pub.p_qv = db_pdb_PutQuadvar;
#endif

    /* Unstructured mesh functions */
    dbfile->pub.g_um = db_pdb_GetUcdmesh;
    dbfile->pub.g_uv = db_pdb_GetUcdvar;
    dbfile->pub.g_fl = db_pdb_GetFacelist;
    dbfile->pub.g_zl = db_pdb_GetZonelist;
    dbfile->pub.g_phzl = db_pdb_GetPHZonelist;
#ifdef PDB_WRITE
    dbfile->pub.p_um = db_pdb_PutUcdmesh;
    dbfile->pub.p_sm = db_pdb_PutUcdsubmesh;
    dbfile->pub.p_uv = db_pdb_PutUcdvar;
    dbfile->pub.p_fl = db_pdb_PutFacelist;
    dbfile->pub.p_zl = db_pdb_PutZonelist;
    dbfile->pub.p_zl2= db_pdb_PutZonelist2;
    dbfile->pub.p_phzl= db_pdb_PutPHZonelist;
#endif

    /* Material functions */
    dbfile->pub.g_ma = db_pdb_GetMaterial;
    dbfile->pub.g_ms = db_pdb_GetMatspecies;
#ifdef PDB_WRITE
    dbfile->pub.p_ma = db_pdb_PutMaterial;
    dbfile->pub.p_ms = db_pdb_PutMatspecies;
#endif

    /* Pointmesh functions */
    dbfile->pub.g_pm = db_pdb_GetPointmesh;
    dbfile->pub.g_pv = db_pdb_GetPointvar;
#ifdef PDB_WRITE
    dbfile->pub.p_pm = db_pdb_PutPointmesh;
    dbfile->pub.p_pv = db_pdb_PutPointvar;
#endif

    /* Multiblock functions */
    dbfile->pub.g_mm = db_pdb_GetMultimesh;
    dbfile->pub.g_mmadj = db_pdb_GetMultimeshadj;
    dbfile->pub.g_mv = db_pdb_GetMultivar;
    dbfile->pub.g_mt = db_pdb_GetMultimat;
    dbfile->pub.g_mms= db_pdb_GetMultimatspecies;
#ifdef PDB_WRITE
    dbfile->pub.p_mm = db_pdb_PutMultimesh;
    dbfile->pub.p_mmadj = db_pdb_PutMultimeshadj;
    dbfile->pub.p_mv = db_pdb_PutMultivar;
    dbfile->pub.p_mt = db_pdb_PutMultimat;
    dbfile->pub.p_mms= db_pdb_PutMultimatspecies;
#endif

    /* Compound arrays */
    dbfile->pub.g_ca = db_pdb_GetCompoundarray;
#ifdef PDB_WRITE
    dbfile->pub.p_ca = db_pdb_PutCompoundarray;
#endif
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_close
 *
 * Purpose:     Closes a PDB file and free the memory associated with
 *              the file.
 *
 * Return:      Success:        NULL, usually assigned to the file
 *                              pointer being closed as in:
 *                                dbfile = db_pdb_close (dbfile) ;
 *
 *              Failure:        Never fails.
 *
 * Programmer:  matzke@viper
 *              Wed Nov  2 13:42:25 PST 1994
 *
 * Modifications:
 *    Eric Brugger, Mon Feb 27 15:55:01 PST 1995
 *    I changed the return value to be an integer instead of a pointer
 *    to a DBfile.
 *
 *    Sean Ahern, Mon Jul  1 14:05:38 PDT 1996
 *    Turned off the PJgroup cache when we close files.
 *
 *    Sean Ahern, Tue Oct 20 17:21:18 PDT 1998
 *    Reformatted whitespace.
 *
 *    Sean Ahern, Mon Nov 23 17:29:17 PST 1998
 *    Added clearing of the object cache when the file is closed.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_close(DBfile *_dbfile)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;

   if (dbfile)
   {
      /*
       * Free the private parts of the file.
       */
      lite_PD_close(dbfile->pdb);
      dbfile->pdb = NULL;

      /* We've closed a file, so we can't cache PJgroups any more */
      PJ_NoCache();

      /*
       * Free the public parts of the file.
       */
      db_close(_dbfile);

      /* Clear the current object cache. */
      PJ_ClearCache();
   }
   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_Open
 *
 * Purpose:     Opens a PDB file that already exists.
 *
 * Return:      Success:        ptr to the file structure
 *
 *              Failure:        NULL, db_errno set.
 *
 * Programmer:  matzke@viper
 *              Wed Nov  2 13:15:16 PST 1994
 *
 * Modifications:
 *    Eric Brugger, Fri Jan 27 08:27:46 PST 1995
 *    I changed the call DBGetToc to db_pdb_GetToc.
 *
 *    Robb Matzke, Tue Mar 7 10:36:13 EST 1995
 *    I changed the call db_pdb_GetToc to DBNewToc.
 *
 *    Sean Ahern, Sun Oct  1 03:05:08 PDT 1995
 *    Made "me" static.
 *
 *    Sean Ahern, Sun Oct  1 06:09:25 PDT 1995
 *    Fixed a parameter type problem.
 *
 *    Sean Ahern, Mon Jan  8 17:37:47 PST 1996
 *    Added the mode parameter and accompanying logic.
 *
 *    Eric Brugger, Fri Jan 19 10:00:41 PST 1996
 *    I checked to see if it is a netcdf flavor of pdb file.
 *    If it is, then it returns NULL.
 *
 *    Sean Ahern, Fri Oct 16 17:46:57 PDT 1998
 *    Reformatted whitespace.
 *-------------------------------------------------------------------------*/
INTERNAL DBfile *
db_pdb_Open(char *name, int mode)
{
    PDBfile        *pdb;
    DBfile_pdb     *dbfile;
    static char    *me = "db_pdb_open";

    if (!SW_file_exists(name))
    {
        db_perror(name, E_NOFILE, me);
        return NULL;
    } else if (!SW_file_readable(name))
    {
        db_perror("not readable", E_NOFILE, me);
        return NULL;
    }
    if (mode == DB_READ)
    {
        if (NULL == (pdb = lite_PD_open(name, "r")))
        {
            db_perror(NULL, E_NOFILE, me);
            return NULL;
        }
    } else if (mode == DB_APPEND)
    {
        if (NULL == (pdb = lite_PD_open(name, "a")))
        {
            db_perror(NULL, E_NOFILE, me);
            return NULL;
        }
    } else
    {
        db_perror("mode", E_INTERNAL, me);
        return (NULL);
    }

    /*
     * If it is the netcdf flavor of pdb, then return NULL.
     */
    if (NULL != lite_SC_lookup("_whatami", pdb->symtab))
    {
        lite_PD_close(pdb);
        return (NULL);
    }

    dbfile = ALLOC(DBfile_pdb);
    memset(dbfile, 0, sizeof(DBfile_pdb));
    dbfile->pub.name = STRDUP(name);
    dbfile->pub.type = DB_PDB;
    dbfile->pdb = pdb;
    db_pdb_InitCallbacks((DBfile *) dbfile);
    return (DBfile *) dbfile;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_Create
 *
 * Purpose:     Creates a PDB file and begins the process of writing
 *              mesh and mesh-related data into that file.
 *
 *              NOTE: The `mode' parameter is unused.
 *                    But it's caught at the DBCreate call.
 *
 * Return:      Success:        pointer to a new file
 *
 *              Failure:        NULL, db_errno set
 *
 * Programmer:  matzke@viper
 *              Thu Nov  3 14:46:38 PST 1994
 *
 * Modifications:
 *    Eric Brugger, Fri Jan 27 08:27:46 PST 1995
 *    I changed the call DBGetToc to db_pdb_GetToc.
 *
 *    Robb Matzke, Tue Mar 7 10:37:07 EST 1995
 *    I changed the call db_pdb_GetToc to DBNewToc.
 *
 *    Sean Ahern, Sun Oct  1 03:05:32 PDT 1995
 *    Made "me" static.
 *
 *    Sean Ahern, Wed Oct  4 17:13:16 PDT 1995
 *    Fixed a parameter type problem.
 *
 *    Sean Ahern, Fri Oct 16 17:47:22 PDT 1998
 *    Reformatted whitespace.
 *
 *    Hank Childs, Thu Jan  6 13:41:38 PST 2000
 *    Put in cast to long of strlen to remove compiler warning.
 *
 *    Jeremy Meredith, Wed Oct 25 16:16:59 PDT 2000
 *    Added DB_INTEL so we had a little-endian target.
 *-------------------------------------------------------------------------*/
/* ARGSUSED */
INTERNAL DBfile *
db_pdb_Create (char *name, int mode, int target, char *finfo)
{
    DBfile_pdb     *dbfile;
    static char    *me = "db_pdb_create";

    if (SILO_Globals.enableChecksums)
    {
        db_perror(name, E_NOTIMP, "no checksums in PDB driver");
        return NULL;
    }

    /*
     * The target type.
     */
    switch (target)
    {
    case DB_LOCAL:
        break;
    case DB_SUN3:
        lite_PD_target(&lite_IEEEA_STD, &lite_M68000_ALIGNMENT);
        break;
    case DB_SUN4:
        lite_PD_target(&lite_IEEEA_STD, &lite_SPARC_ALIGNMENT);
        break;
    case DB_SGI:
        lite_PD_target(&lite_IEEEA_STD, &lite_MIPS_ALIGNMENT);
        break;
    case DB_RS6000:
        lite_PD_target(&lite_IEEEA_STD, &lite_RS6000_ALIGNMENT);
        break;
    case DB_CRAY:
        lite_PD_target(&lite_CRAY_STD, &lite_UNICOS_ALIGNMENT);
        break;
    case DB_INTEL:
        lite_PD_target(&lite_IEEEA_STD, &lite_INTELA_ALIGNMENT);
        break;
    default:
        db_perror("target", E_BADARGS, me);
        return NULL;
    }

    if (NULL == (dbfile = ALLOC(DBfile_pdb)))
    {
        db_perror(name, E_NOMEM, me);
        return NULL;
    }
    dbfile->pub.name = STRDUP(name);
    dbfile->pub.type = DB_PDB;
    db_pdb_InitCallbacks((DBfile *) dbfile);

    if (NULL == (dbfile->pdb = lite_PD_open(name, "w")))
    {
        FREE(dbfile->pub.name);
        db_perror(name, E_NOFILE, me);
        return NULL;
    }
    lite_PD_mkdir(dbfile->pdb, "/");
    DBNewToc((DBfile *) dbfile);
    if (finfo)
    {
        long    count = (long) strlen(finfo) + 1;

        PJ_write_len(dbfile->pdb, "_fileinfo", "char", finfo, 1, &count);
    }
    return (DBfile *) dbfile;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_ForceSingle
 *
 * Purpose:     If status is non-zero, then force all double-precision
 *              floating-point values to single precision.
 *
 * Return:      Success:        0
 *
 *              Failure:        never fails
 *
 * Programmer:  matzke@viper
 *              Tue Jan 10 10:50:49 PST 1995
 *
 * Modifications:
 *-------------------------------------------------------------------------*/
INTERNAL int
db_pdb_ForceSingle (int status)
{
   PJ_ForceSingle(status);
   return 0;
}

/*--------------------------------------------------------------------
 *  Routine                                                      PJ_ls
 *
 *  Purpose
 *
 *      Return a list of all variables of the specified type in the
 *      specified directory.
 *
 *  Notes
 *
 *      Providing a NULL string for 'type' will cause the type check
 *      to be avoided. Providing a NULL string for the directory path
 *      will cause only the root directory to be searched.
 *
 *  Modifications
 *
 *      Robb Matzke, Fri Feb 24 10:37:43 EST 1995
 *      This function just calls PD_ls().  But we must make sure that
 *      the return value is allocated with the C memory management.
 *
 *      Robb Matzke, Fri Dec 2 13:20:38 PST 1994
 *      Removed references to SCORE memory management.  The varlist
 *      vector and the strings in that vector are allocated with
 *      SCORE by lite_SC_hash_dump()--actually, the strings pointed to
 *      by this vector should not be freed because they are in the
 *      hash table.
 *
 *      Al Leibee, Mon Aug  9 10:30:54 PDT 1993
 *      Convert to new PD_inquire_entry interface.
 *
 *      Al Leibee, Thu Jul  8 08:05:09 PDT 1993
 *      FREE to SCFREE to be consistant with allocation.
 *--------------------------------------------------------------------*/
INTERNAL char **
PJ_ls (PDBfile       *file, /* PDB file pointer */
       char          *path, /* Path of dir to search (if NULL use '/') */
       char          *type, /* Variable type to search for (else NULL) */
       int           *num)  /* Returned number of matches */
{
   char         **score_out, **malloc_out;

   score_out = lite_PD_ls(file, path, type, num);
   malloc_out = ALLOC_N(char *, *num + 1);
   memcpy(malloc_out, score_out, *num * sizeof(char *));

   malloc_out[*num] = NULL;
   lite_SC_free(score_out);
   return malloc_out;
}

/*--------------------------------------------------------------------
 *  Routine                                            PJ_get_fullpath
 *
 *  Purpose
 *
 *      Generate a full pathname from the current working directory
 *      and the given pathname (abs/rel)
 *
 *  Notes
 *
 *      This routine is for internal use only. Not for user-level.
 *
 *      Robb Matzke, Fri Feb 24 10:40:39 EST 1995
 *      Removed the call to PJ_file_has_dirs() and we assume that all
 *      files support directories.
 *
 *      Sean Ahern, Fri Oct 16 17:47:44 PDT 1998
 *      Reformatted whitespace.
 *--------------------------------------------------------------------*/
INTERNAL int
PJ_get_fullpath(
   PDBfile       *file,
   char          *cwd,         /* Current working directory */
   char          *path,        /* Pathname (abs or rel) to traverse */
   char          *name)        /* Returned adjusted name */
{
    int             ierr;
    char           *subpath;
    char            tmpstr[256];

    if (file == NULL || cwd == NULL || path == NULL || name == NULL)
        return (FALSE);

    ierr = 0;
    name[0] = '\0';

    /*
     *  If using an absolute path just copy verbatim.
     */
    if (path[0] == '/')
    {
        strcpy(name, path);
    } else
    {

        /*
         *  Using a relative path.
         *  Break path into slash-separated tokens, and process
         *  each subpath individually.
         */

        strcpy(name, cwd);
        strcpy(tmpstr, path);

        subpath = (char *)strtok(tmpstr, "/");

        while (subpath != NULL && !ierr)
        {

            if (STR_EQUAL("/", subpath))
            {

                /* No-op */

            } else if (STR_EQUAL(".", subpath))
            {

                /* No-op */

            } else if (STR_EQUAL("..", subpath))
            {

                /* Go up one level, unless already at top */
                if (!STR_EQUAL("/", cwd))
                {
                    char           *s;

                    s = strrchr(name, '/');
                    if (s != NULL)
                        s[0] = '\0';
                }
            } else
            {

                /* Append to end of current path */
                if (STR_LASTCHAR(name) != '/')
                    strcat(name, "/");
                strcat(name, subpath);
            }
            subpath = (char *)strtok(NULL, "/");
        }
    }

    if (name[0] == '\0')
        strcpy(name, "/");

    return (TRUE);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_getobjinfo
 *
 * Purpose:
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *    Al Leibee, Fri Sep  3 10:54:47 PDT 1993
 *    Error check on PJ_read.
 *
 *    Eric Brugger, Thu Feb  9 12:54:37 PST 1995
 *    I modified the routine to read elements of the group structure
 *    as "abc->type" instead of "abc.type".
 *
 *    Sean Ahern, Sun Oct  1 03:05:56 PDT 1995
 *    Made "me" static.
 *
 *    Eric Brugger, Fri Dec  4 12:45:22 PST 1998
 *    Added code to free ctype to eliminate a memory leak.
 *-------------------------------------------------------------------------*/
PRIVATE int
db_pdb_getobjinfo (PDBfile       *pdb,
                   char          *name, /* Name of object to inquire about */
                   char          *type, /* Returned object type of 'name'  */
                   int           *num)  /* Returned number of elements */
{
   char           newname[MAXNAME];
   char          *ctype;
   static char   *me = "db_pdb_getobjinfo";

   if (!pdb)
      return db_perror(NULL, E_NOFILE, me);
   if (!name || !*name)
      return db_perror("name", E_BADARGS, me);

   *num = *type = 0;

   sprintf(newname, "%s->type", name);
   if (PJ_read(pdb, newname, &ctype) == FALSE) {
      return db_perror("PJ_read", E_CALLFAIL, me);
   }
   strcpy(type, ctype);
   SCFREE(ctype);

   sprintf(newname, "%s->ncomponents", name);
   PJ_read(pdb, newname, num);

   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_getvarinfo
 *
 * Purpose:
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *
 *      Al Leibee, Wed Aug 18 15:59:26 PDT 1993
 *      Convert to new PJ_inquire_entry interface.
 *
 *      Al Leibee, Mon Aug 30 15:13:50 PDT 1993
 *      Use PD_inquire_host_type to get true data type size.
 *
 *      Sean Ahern, Sun Oct  1 03:06:20 PDT 1995
 *      Made "me" static.
 *
 *      Sean Ahern, Wed Apr 12 11:14:38 PDT 2000
 *      Removed the last two parameters to PJ_inquire_entry because they
 *      weren't being used.
 *-------------------------------------------------------------------------*/
PRIVATE int
db_pdb_getvarinfo (PDBfile       *pdb,
                   char          *name,     /*Name of var to inquire about */
                   char          *type,     /*Returned datatype of 'name' */
                   int           *num,      /*Returned number of elements */
                   int           *size,     /*Returned element size */
                   int            verbose)  /*Sentinel: 1=print error msgs*/
{
   int            is_ptr;
   char           lastchar;
   char          *s;
   defstr        *dp;
   syment        *ep;
   static char   *me = "db_pdb_getvarinfo";

   *num = *size = 0;
   if (type)
      type[0] = '\0';

   /*
    *  Get symbol table entry for requested variable.
    */
   ep = PJ_inquire_entry(pdb, name);
   if (ep == NULL)
      return db_perror("PJ_inquire_entry", E_CALLFAIL, me);

   /* Assign values */
   if (type)
      strcpy(type, ep->type);

   lastchar = STR_LASTCHAR(ep->type);
   is_ptr = (lastchar == '*');

   if (is_ptr) {
      s = ALLOC_N(char, strlen(ep->type) + 1);

      strcpy(s, ep->type);
      s[strcspn(s, " *")] = '\0';

      /* Get data size of primitive (non-pointer) unit */
      dp = PD_inquire_host_type(pdb, s);
      *size = dp->size;
      *num = -1;              /* lite_SC_arrlen(values) / dp->size; */
      if (verbose)
         printf("Cannot query length of pointered variable.\n");

      FREE(s);

   }
   else {
      dp = PD_inquire_host_type(pdb, ep->type);
      if (dp == NULL) {
         if (verbose)
            printf("Don't know about data of type: %s\n", ep->type);
         return db_perror("PD_inquire_host_type", E_CALLFAIL, me);
      }

      /* Assign values */
      *size = dp->size;
      *num = ep->number;
   }

   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetDir
 *
 * Purpose:     Return the name of the current directory by copying the
 *              name to the output buffer supplied.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  matzke@viper
 *              Tue Nov  8 08:52:38 PST 1994
 *
 * Modifications:
 *    Sean Ahern, Sun Oct  1 03:06:52 PDT 1995
 *    Made "me" static.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetDir (DBfile *_dbfile, char *result)
{
   char          *p;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_GetDir";

   if (!result)
      return db_perror("result", E_BADARGS, me);
   p = lite_PD_pwd(dbfile->pdb);
   if (!p || !*p) {
      db_perror("PD_pwd", E_CALLFAIL, me);
      result[0] = '\0';
      return -1;
   }
   strcpy(result, p);
   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_NewToc
 *
 * Purpose:     Read the table of contents from the current directory
 *              and make it the current table of contents for the file
 *              pointer, freeing any previously existing table of contents.
 *
 * Notes
 *
 *              It is assumed that scalar values within the TOC have been
 *              initialized to zero.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1, db_errno set
 *
 * Programmer:  matzke@viper
 *              Thu Nov  3 14:34:31 PST 1994
 *
 * Modifications
 *    Al Leibee, Wed Jul  7 08:00:00 PDT 1993
 *    Changed FREE to SCFREE for consistant allocate/free usage.
 *
 *    Al Leibee, Wed Aug 18 15:59:26 PDT 1993
 *    Convert to new PJ_inquire_entry interface.
 *
 *    Al Leibee, Mon Aug  1 16:16:00 PDT 1994
 *    Added material species.
 *
 *    Robb Matzke, Tue Oct 25 09:46:48 PDT 1994
 *    added compound arrays.
 *
 *    Robb Matzke, Fri Dec 2 14:05:57 PST 1994
 *    Removed all references to SCORE memory management.  Local variable
 *    `name' is now allocated on the stack instead of by memory management.
 *
 *    Eric Brugger, Fri Jan 27 08:27:46 PST 1995
 *    I made it into an internal routine.
 *
 *    Eric Brugger, Thu Feb  9 12:54:37 PST 1995
 *    I modified the routine to read elements of the group structure
 *    as "abc->type" instead of "abc.type".
 *
 *    Eric Brugger, Thu Feb  9 15:07:29 PST 1995
 *    I modified the routine to handle the obj in the table of contents.
 *
 *    Robb Matzke, Tue Feb 21 16:20:58 EST 1995
 *    Removed references to the `id' fields of the DBtoc.
 *
 *    Robb Matzke, Tue Mar 7 10:38:59 EST 1995
 *    Changed the name from db_pdb_GetToc to db_pdb_NewToc.
 *
 *    Robb Matzke, Tue Mar 7 11:21:21 EST 1995
 *    Changed this to a CALLBACK.
 *
 *    Katherine Price, Thu May 25 14:44:42 PDT 1995
 *    Added multi-block materials.
 *
 *    Eric Brugger, Thu Jun 29 09:44:47 PDT 1995
 *    I modified the routine so that directories would show up in the
 *    table of contents.
 *
 *    Sean Ahern, Sun Oct  1 03:07:17 PDT 1995
 *    Made "me" static.
 *
 *    Eric Brugger, Mon Oct 23 08:00:16 PDT 1995
 *    I corrected a bug where the last character of a directory name
 *    was always eliminated.  This was done to get rid to the terminating
 *    '/' character in a directory name.  Well it turns out that not
 *    all silo files have a '/' at the end of a directory name.
 *
 *    Jeremy Meredith, Sept 18 1998
 *    Added multi-block species.
 *
 *    Jeremy Meredith, Nov 17 1998
 *    Added initialization of imultimatspecies.
 *
 *    Eric Brugger, Fri Dec  4 12:45:22 PST 1998
 *    Added code to free ctype to eliminate a memory leak.
 *
 *    Hank Childs, Thu Jan  6 13:33:21 PST 2000
 *    Removed print statement that appears to be a debugging relic.
 *
 *    Robb Matzke, Fri May 19 13:18:36 EDT 2000
 *    Avoid malloc(0)/calloc(0) since the behavior is undefined by Posix.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_NewToc (DBfile *_dbfile)
{
#define PJDIR  -10
#define PJVAR  -11

   int            i, lstr, num;
   int           *types=NULL;
   int            ivar, iqmesh, iqvar, iumesh, iuvar, icurve, idir, iarray,
      imat, imatspecies, imultimesh, imultivar, imultimat, imultimatspecies,
      ipmesh, iptvar, iobj, icsgmesh, icsgvar, idefvars, imultimeshadj;

   DBtoc         *toc;
   char          *ctype;
   char         **list;        /* Names of everything in current directory */
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PDBfile       *file;        /* PDB file pointer  */
   char           name[128];
   static char   *me = "db_pdb_NewToc";

   db_FreeToc(_dbfile);
   dbfile->pub.toc = toc = db_AllocToc();

   file = dbfile->pdb;

   /*------------------------------------------------------------
    *  Get count of each entity (var, dir, curve, etc.)
    *  Start by getting list of everything in current dir,
    *  then count occurences of each type. Build an array of types
    *  which correspond to each item in list.
    *  (list is allocated by malloc().  The strings it points
    *  to should never be free since they are the strings stored in
    *  the SCORE hash table used by PDB.
    *------------------------------------------------------------*/
   list = PJ_ls(file, ".", NULL, &num);
   if (num) types = ALLOC_N(int, num);

   for (i = 0; i < num; i++) {

      syment        *ep;

      ep = lite_PD_inquire_entry(file, list[i], TRUE, NULL);

      if (ep == NULL) {
         types[i] = 999999;
         continue;
      }

      /*----------------------------------------
       *  Directory
       *----------------------------------------*/
      if (STR_BEGINSWITH(ep->type, "Directory")) {
         toc->ndir++;
         types[i] = PJDIR;

         /*----------------------------------------
          *  Group (object)
          *----------------------------------------*/
      }
      else if (STR_BEGINSWITH(ep->type, "Group")) {

         /*
          * Read the type field of each object and increment
          * the appropriate count.
          */
         sprintf(name, "%s.type", list[i]);
         if (!PJ_read(file, name, &ctype)) {
            sprintf(name, "%s->type", list[i]);
            if (!PJ_read(file, name, &ctype)) {
               return db_perror("PJ_read", E_CALLFAIL, me);
            }
         }

         types[i] = DBGetObjtypeTag(ctype);
         SCFREE(ctype);

         switch (types[i]) {
         case DB_MULTIMESH:
            toc->nmultimesh++;
            break;
         case DB_MULTIMESHADJ:
            toc->nmultimeshadj++;
            break;
         case DB_MULTIVAR:
            toc->nmultivar++;
            break;
         case DB_MULTIMAT:
            toc->nmultimat++;
            break;
         case DB_MULTIMATSPECIES:
            toc->nmultimatspecies++;
            break;
         case DB_CSGMESH:
            toc->ncsgmesh++;
            break;
         case DB_CSGVAR:
            toc->ncsgvar++;
            break;
         case DB_DEFVARS:
            toc->ndefvars++;
            break;
         case DB_QUADMESH:
         case DB_QUAD_RECT:
         case DB_QUAD_CURV:
            toc->nqmesh++;
            break;
         case DB_QUADVAR:
            toc->nqvar++;
            break;
         case DB_UCDMESH:
            toc->nucdmesh++;
            break;
         case DB_UCDVAR:
            toc->nucdvar++;
            break;
         case DB_POINTMESH:
            toc->nptmesh++;
            break;
         case DB_POINTVAR:
            toc->nptvar++;
            break;
         case DB_CURVE:
            toc->ncurve++;
            break;
         case DB_MATERIAL:
            toc->nmat++;
            break;
         case DB_MATSPECIES:
            toc->nmatspecies++;
            break;
         case DB_ARRAY:
            toc->narrays++;
            break;
         default:
            toc->nobj++;
            break;
         }

         /*----------------------------------------
          *  Other (variable?)
          *----------------------------------------*/
      }
      else {
         types[i] = PJVAR;
         toc->nvar++;
      }
   }

   /*----------------------------------------------------------------------
    *  Now all the counts have been made; allocate space.
    *---------------------------------------------------------------------*/
   if (toc->nvar > 0) {
      toc->var_names = ALLOC_N(char *, toc->nvar);
   }

   if (toc->nobj > 0) {
      toc->obj_names = ALLOC_N(char *, toc->nobj);
   }

   if (toc->ndir > 0) {
      toc->dir_names = ALLOC_N(char *, toc->ndir);
   }

   if (toc->ncurve > 0) {
      toc->curve_names = ALLOC_N(char *, toc->ncurve);
   }

   if (toc->ndefvars > 0) {
      toc->defvars_names = ALLOC_N(char *, toc->ndefvars);
   }

   if (toc->nmultimesh > 0) {
      toc->multimesh_names = ALLOC_N(char *, toc->nmultimesh);
   }

   if (toc->nmultimeshadj > 0) {
      toc->multimeshadj_names = ALLOC_N(char *, toc->nmultimeshadj);
   }

   if (toc->nmultivar > 0) {
      toc->multivar_names = ALLOC_N(char *, toc->nmultivar);
   }

   if (toc->nmultimat > 0) {
      toc->multimat_names = ALLOC_N(char *, toc->nmultimat);
   }

   if (toc->nmultimatspecies > 0) {
      toc->multimatspecies_names = ALLOC_N(char *, toc->nmultimatspecies);
   }

   if (toc->ncsgmesh > 0) {
      toc->csgmesh_names = ALLOC_N(char *, toc->ncsgmesh);
   }

   if (toc->ncsgvar > 0) {
      toc->csgvar_names = ALLOC_N(char *, toc->ncsgvar);
   }

   if (toc->nqmesh > 0) {
      toc->qmesh_names = ALLOC_N(char *, toc->nqmesh);
   }

   if (toc->nqvar > 0) {
      toc->qvar_names = ALLOC_N(char *, toc->nqvar);
   }

   if (toc->nucdmesh > 0) {
      toc->ucdmesh_names = ALLOC_N(char *, toc->nucdmesh);
   }

   if (toc->nucdvar > 0) {
      toc->ucdvar_names = ALLOC_N(char *, toc->nucdvar);
   }

   if (toc->nptmesh > 0) {
      toc->ptmesh_names = ALLOC_N(char *, toc->nptmesh);
   }

   if (toc->nptvar > 0) {
      toc->ptvar_names = ALLOC_N(char *, toc->nptvar);
   }

   if (toc->nmat > 0) {
      toc->mat_names = ALLOC_N(char *, toc->nmat);
   }

   if (toc->nmatspecies > 0) {
      toc->matspecies_names = ALLOC_N(char *, toc->nmatspecies);
   }

   if (toc->narrays > 0) {
      toc->array_names = ALLOC_N(char *, toc->narrays);
   }

   /*----------------------------------------------------------------------
    *  Now loop over all the items in the directory and store the
    *  names and ID's
    *---------------------------------------------------------------------*/
   icurve = ivar = iqmesh = iqvar = iumesh = iuvar = idir = iarray = 0;
   imultimesh = imultivar = imultimat = imat = imatspecies = ipmesh = 0;
   iptvar = iobj = imultimatspecies = icsgmesh = icsgvar = idefvars = 0;
   imultimeshadj = 0;

   for (i = 0; i < num; i++) {

      switch (types[i]) {

      case 999999:
         /*The call to PD_inquire_entry() failed above. */
         break;

      case PJDIR:
         /*
          * After copying the directory name, eliminate
          * the terminating '/' if one is present.
          */
         toc->dir_names[idir] = STRDUP(list[i]);
         lstr = strlen(list[i]);
         if (toc->dir_names[idir][lstr-1] == '/')
            toc->dir_names[idir][lstr-1] = '\0';
         idir++;
         break;

      case PJVAR:
         toc->var_names[ivar] = STRDUP(list[i]);
         ivar++;
         break;

      case DB_MULTIMESH:
         toc->multimesh_names[imultimesh] = STRDUP(list[i]);
         imultimesh++;
         break;

      case DB_MULTIMESHADJ:
         toc->multimeshadj_names[imultimeshadj] = STRDUP(list[i]);
         imultimeshadj++;
         break;

      case DB_MULTIVAR:
         toc->multivar_names[imultivar] = STRDUP(list[i]);
         imultivar++;
         break;

      case DB_MULTIMAT:
         toc->multimat_names[imultimat] = STRDUP(list[i]);
         imultimat++;
         break;

      case DB_MULTIMATSPECIES:
         toc->multimatspecies_names[imultimatspecies] = STRDUP(list[i]);
         imultimatspecies++;
         break;

      case DB_CSGMESH:
         toc->csgmesh_names[icsgmesh] = STRDUP(list[i]);
         icsgmesh++;
         break;

      case DB_CSGVAR:
         toc->csgvar_names[icsgvar] = STRDUP(list[i]);
         icsgvar++;
         break;

      case DB_DEFVARS:
         toc->defvars_names[idefvars] = STRDUP(list[i]);
         idefvars++;
         break;

      case DB_QUAD_RECT:
      case DB_QUAD_CURV:
      case DB_QUADMESH:
         toc->qmesh_names[iqmesh] = STRDUP(list[i]);
         iqmesh++;
         break;

      case DB_QUADVAR:
         toc->qvar_names[iqvar] = STRDUP(list[i]);
         iqvar++;
         break;

      case DB_UCDMESH:
         toc->ucdmesh_names[iumesh] = STRDUP(list[i]);
         iumesh++;
         break;

      case DB_UCDVAR:
         toc->ucdvar_names[iuvar] = STRDUP(list[i]);
         iuvar++;
         break;

      case DB_POINTMESH:
         toc->ptmesh_names[ipmesh] = STRDUP(list[i]);
         ipmesh++;
         break;

      case DB_POINTVAR:
         toc->ptvar_names[iptvar] = STRDUP(list[i]);
         iptvar++;
         break;

      case DB_CURVE:
         toc->curve_names[icurve] = STRDUP(list[i]);
         icurve++;
         break;

      case DB_MATERIAL:
         toc->mat_names[imat] = STRDUP(list[i]);
         imat++;
         break;

      case DB_MATSPECIES:
         toc->matspecies_names[imatspecies] = STRDUP(list[i]);
         imatspecies++;
         break;

      case DB_ARRAY:
         toc->array_names[iarray] = STRDUP(list[i]);
         iarray++;
         break;

      default:
         toc->obj_names[iobj] = STRDUP(list[i]);
         iobj++;
         break;
      }
   }

   FREE(list);
   if (types) FREE(types);
   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_InqVarType
 *
 * Purpose:     Return the DBObjectType for a given object name
 *
 * Return:      Success:        the ObjectType for the given object
 *
 *              Failure:        DB_INVALID_OBJECT
 *
 * Programmer:  Sean Ahern,
 *              Wed Oct 28 14:46:53 PST 1998
 *
 * Modifications:
 *    Eric Brugger, Fri Dec  4 12:45:22 PST 1998
 *    Added code to free char_type to eliminate a memory leak.
 *
 *    Sean Ahern, Wed Dec  9 16:21:52 PST 1998
 *    Added some checks of the return value of DBGetObjtypeTag to make sure
 *    that we're returning a DBObjectType.
 *
 *    Sean Ahern, Thu Apr 29 16:24:16 PDT 1999
 *    Added two checks, one is if the variable exists at all.  The other is to
 *    add some logic to help PDB understand directories.
 *
 *    Lisa J. Roberts, Tue Nov 23 09:49:42 PST 1999
 *    Removed type, which was unused.
 *
 *    Hank Childs, Thu Jan  6 13:46:02 PST 2000
 *    Put in cast to remove compiler warning.
 *-------------------------------------------------------------------------*/
CALLBACK DBObjectType
db_pdb_InqVarType(DBfile *_dbfile, char *varname)
{
    DBfile_pdb     *dbfile = (DBfile_pdb *) _dbfile;
    PDBfile        *file = dbfile->pdb;
    char           *char_type = NULL;
    char            name[256];
    syment         *entry;
    int             typetag;

    /* First, check to see if it exists */
    entry = lite_PD_inquire_entry(file, varname, TRUE, NULL);
    if (entry == NULL)
    {
        /* This could be a directory.  Add a "/" to the end and try again. */
        char *newname = (char*)malloc(strlen(varname)+2);
        sprintf(newname,"%s/",varname);
        entry = lite_PD_inquire_entry(file, newname, TRUE, NULL);
        free(newname);

        /* If it's NULL now, we really have an invalid object.  Otherwise,
         * just drop out of the if, and continue. */
        if (entry == NULL)
            return(DB_INVALID_OBJECT);
    }

    /* Then, check to see if it's a directory. */
    if (STR_BEGINSWITH(entry->type, "Directory"))
        return(DB_DIR);
    else if (STR_BEGINSWITH(entry->type, "Group"))
    {
        /* It's not a directory.  Ask PDB what the type is. */
        sprintf(name, "%s.type", varname);
        if (!PJ_read(file, name, &char_type))
        {
            sprintf(name, "%s->type", varname);
            if (!PJ_read(file, name, &char_type))
                return(DB_INVALID_OBJECT);
        }
        typetag = DBGetObjtypeTag(char_type);
        SCFREE(char_type);
        if ((typetag == DB_QUAD_RECT) || (typetag == DB_QUAD_CURV))
            typetag = DB_QUADMESH;
        return( (DBObjectType) typetag);
    } else
    {
        return(DB_VARIABLE);
    }

    /* For stupid compilers */
    /* NOTREACHED */
    return(DB_INVALID_OBJECT);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetAtt
 *
 * Purpose:     Allocate space for, and read, the given attribute of the
 *              given variable.
 *
 * Return:      Success:        pointer to result
 *
 *              Failure:        NULL
 *
 * Programmer:  matzke@viper
 *              Tue Nov  8 08:06:45 PST 1994
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:07:41 PDT 1995
 *     Made "me" static.
 *-------------------------------------------------------------------------*/
CALLBACK void *
db_pdb_GetAtt (DBfile *_dbfile, char *varname, char *attname)
{
   void          *result;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_GetAtt";

   result = lite_PD_get_attribute(dbfile->pdb, varname, attname);
   if (!result) {
      db_perror("PD_get_attribute", E_CALLFAIL, me);
      return NULL;
   }
   return result;
}


/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetObject
 *
 * Purpose:     Reads a group from a PDB file.
 *
 * Return:      Success:        Ptr to the new group, all memory allocated
 *                              by malloc().
 *
 *              Failure:        NULL
 *
 * Programmer:  Robb Matzke
 *              robb@callisto.nuance.mdn.com
 *              Jan 24, 1997
 *
 * Modifications:
 *
 *    Lisa J. Roberts, Tue Nov 23 09:39:49 PST 1999
 *    Changed strdup to safe_strdup.
 *-------------------------------------------------------------------------*/
CALLBACK DBobject *
db_pdb_GetObject (DBfile *_file, char *name)
{
   PJgroup      *group=NULL;
   PDBfile      *file = ((DBfile_pdb *)_file)->pdb;
   DBobject     *obj=NULL;
   int          i;

   if (!PJ_get_group (file, name, &group)) return NULL;

   /*
    * We build our own DBobject here instead of calling DBMakeObject
    * because we have a character string type name instead of an
    * integer type constant.
    */
   obj = malloc (sizeof (DBobject));
   obj->name = safe_strdup (group->name);
   obj->type = safe_strdup (group->type);
   obj->ncomponents = obj->maxcomponents = group->ncomponents;
   obj->comp_names = malloc (obj->maxcomponents * sizeof(char*));
   obj->pdb_names  = malloc (obj->maxcomponents * sizeof(char*));

   for (i=0; i<group->ncomponents; i++) {
      obj->comp_names[i] = safe_strdup (group->comp_names[i]);
      obj->pdb_names[i] = safe_strdup (group->pdb_names[i]);
   }

#if 0
   /*
    * The PJ_rel_group causes things to break!
    */
   PJ_rel_group (group);
#endif

   return obj;
}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_GetMaterial
 *
 *  Purpose
 *
 *      Read a material-data object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modified
 *
 *      Robb Matzke, Mon Nov 14 14:30:12 EST 1994
 *      Device independence rewrite.
 *
 *      Sean Ahern, Sun Oct  1 03:07:59 PDT 1995
 *      Made "me" static.
 *
 *      Robb Matzke, 26 Aug 1997
 *      The datatype is set from DB_DOUBLE to DB_FLOAT only if the
 *      force-single flag is set.
 *
 *      Sean Ahern, Wed Jun 14 10:53:56 PDT 2000
 *      Added a check to make sure the object we're reading is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Sean Ahern, Tue Feb  5 13:35:49 PST 2002
 *      Added support for material names.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *--------------------------------------------------------------------*/
CALLBACK DBmaterial *
db_pdb_GetMaterial(DBfile *_dbfile,     /*DB file pointer */
                   char   *name)        /*Name of material object to return*/
{
    DBmaterial *mm = NULL;
    DBfile_pdb *dbfile = (DBfile_pdb *) _dbfile;
    static char *me = "db_pdb_GetMaterial";
    PJcomplist tmp_obj;
    char   *type = NULL;
    char *tmpnames = NULL;
    char *tmpcolors = NULL;
    DBmaterial tmpmm;
    memset(&tmpmm, 0, sizeof(DBmaterial));

    /* Comp. Name        Comp. Address     Data Type     */
    INIT_OBJ(&tmp_obj);

    DEFINE_OBJ("ndims",       &tmpmm.ndims,       DB_INT);
    DEFINE_OBJ("dims",         tmpmm.dims,        DB_INT);
    DEFINE_OBJ("major_order", &tmpmm.major_order, DB_INT);
    DEFINE_OBJ("origin",      &tmpmm.origin,      DB_INT);
    DEFALL_OBJ("meshid",      &tmpmm.meshname,    DB_CHAR);
    DEFINE_OBJ("guihide",     &tmpmm.guihide,     DB_INT);

    DEFINE_OBJ("nmat",        &tmpmm.nmat,        DB_INT);
    DEFINE_OBJ("mixlen",      &tmpmm.mixlen,      DB_INT);
    DEFINE_OBJ("datatype",    &tmpmm.datatype,    DB_INT);

    if (SILO_Globals.dataReadMask & DBMatMatnos)
        DEFALL_OBJ("matnos",      &tmpmm.matnos,      DB_INT);
    if (SILO_Globals.dataReadMask & DBMatMatnames)
        DEFALL_OBJ("matnames",    &tmpnames,        DB_CHAR);
    if (SILO_Globals.dataReadMask & DBMatMatcolors)
        DEFALL_OBJ("matcolors",    &tmpcolors,        DB_CHAR);
    if (SILO_Globals.dataReadMask & DBMatMatlist)
        DEFALL_OBJ("matlist",     &tmpmm.matlist,     DB_INT);
    if (SILO_Globals.dataReadMask & DBMatMixList)
    {
        DEFALL_OBJ("mix_mat",     &tmpmm.mix_mat,     DB_INT);
        DEFALL_OBJ("mix_next",    &tmpmm.mix_next,    DB_INT);
        DEFALL_OBJ("mix_zone",    &tmpmm.mix_zone,    DB_INT);
        DEFALL_OBJ("mix_vf",      &tmpmm.mix_vf,      DB_FLOAT);
    }

    if (PJ_GetObject(dbfile->pdb, name, &tmp_obj, &type) < 0)
        return NULL;

    if (NULL == (mm = DBAllocMaterial()))
    {
        db_perror("DBAllocMaterial", E_CALLFAIL, me);
        return NULL;
    }
    *mm = tmpmm;

    CHECK_TYPE(type,DB_MATERIAL,name);
    _DBQQCalcStride(mm->stride, mm->dims, mm->ndims, mm->major_order);

    /* If we have material names, restore it to an array of names.  In the
     * file, it's stored as one string, with individual names separated by
     * semicolons. */
    if ((tmpnames != NULL) && (mm->nmat > 0))
    {
        char *s, *name;
        int i;
        char error[256];

        mm->matnames = ALLOC_N(char *, mm->nmat);

        s = &tmpnames[0];
        name = (char *)strtok(s, ";");

        for (i = 0; i < mm->nmat; i++)
        {
            mm->matnames[i] = STRDUP(name);

            if (i + 1 < mm->nmat)
            {
                name = (char *)strtok(NULL, ";");
                if (name == NULL)
                {
                    sprintf(error, "(%s) Not enough material names found\n", me);
                    db_perror(error, E_INTERNAL, me);
                }
            }
        }
        FREE(tmpnames);
    }
    if ((tmpcolors != NULL) && (mm->nmat > 0))
    {
        mm->matcolors = db_StringListToStringArray(tmpcolors, mm->nmat);
        FREE(tmpcolors);
    }

    mm->id = 0;
    mm->name = STRDUP(name);
    if (DB_DOUBLE == mm->datatype && PJ_InqForceSingle())
    {
        mm->datatype = DB_FLOAT;
    }

    return (mm);
}

/*----------------------------------------------------------------------
 *  Routine                                       db_pdb_GetMatspecies
 *
 *  Purpose
 *
 *      Read a matspecies-data object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modifications
 *
 *      Robb Matzke, Tue Nov 29 13:29:57 PST 1994
 *      Changed for device independence.
 *
 *      Al Leibee, Tue Jul 26 08:44:01 PDT 1994
 *      Replaced composition by species.
 *
 *      Sean Ahern, Sun Oct  1 03:08:19 PDT 1995
 *      Made "me" static.
 *
 *      Jeremy Meredith, Wed Jul  7 12:15:31 PDT 1999
 *      I removed the origin value from the species object.
 *
 *      Sean Ahern, Wed Jun 14 17:19:12 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *--------------------------------------------------------------------*/
CALLBACK DBmatspecies *
db_pdb_GetMatspecies (DBfile *_dbfile,   /*DB file pointer */
                      char   *objname)   /*Name of matspecies obj to return */
{
   char *type = NULL;
   DBmatspecies  *mm;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_GetMatspecies";
   char           tmpstr[256];
   PJcomplist     tmp_obj;
   DBmatspecies   tmpmm;
   memset(&tmpmm, 0, sizeof(DBmatspecies));

   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   DEFALL_OBJ("matname", &tmpmm.matname, DB_CHAR);
   DEFINE_OBJ("ndims", &tmpmm.ndims, DB_INT);
   DEFINE_OBJ("dims", tmpmm.dims, DB_INT);
   DEFINE_OBJ("major_order", &tmpmm.major_order, DB_INT);
   DEFINE_OBJ("datatype", &tmpmm.datatype, DB_INT);
   DEFINE_OBJ("nmat", &tmpmm.nmat, DB_INT);
   DEFALL_OBJ("nmatspec", &tmpmm.nmatspec, DB_INT);
   DEFINE_OBJ("nspecies_mf", &tmpmm.nspecies_mf, DB_INT);
   DEFALL_OBJ("speclist", &tmpmm.speclist, DB_INT);
   DEFINE_OBJ("mixlen", &tmpmm.mixlen, DB_INT);
   DEFALL_OBJ("mix_speclist", &tmpmm.mix_speclist, DB_INT);
   DEFINE_OBJ("guihide", &tmpmm.guihide, DB_INT);

   if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
      return NULL;

   if (NULL == (mm = DBAllocMatspecies())) {
      db_perror("DBAllocMatspecies", E_CALLFAIL, me);
      return NULL;
   }
   *mm = tmpmm;

   CHECK_TYPE(type, DB_MATSPECIES, objname);

   /* Now read in species_mf per its datatype. */
   INIT_OBJ(&tmp_obj);

   if (mm->datatype == 0) {
      strcpy(tmpstr, objname);
      strcat(tmpstr, "_data");
      mm->datatype = db_pdb_GetVarDatatype(dbfile->pdb, tmpstr);

      if (mm->datatype < 0)
         mm->datatype = DB_FLOAT;
   }

   if (mm->datatype == DB_DOUBLE && PJ_InqForceSingle())
      mm->datatype = DB_FLOAT;

   DEFALL_OBJ("species_mf", &mm->species_mf, mm->datatype);
   PJ_GetObject(dbfile->pdb, objname, &tmp_obj, NULL);

   _DBQQCalcStride(mm->stride, mm->dims, mm->ndims, mm->major_order);

   mm->id = 0;
   mm->name = STRDUP(objname);

   return (mm);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetCompoundarray
 *
 * Purpose:     Read a compound array object from a PDB data file.
 *
 * Return:      Success:        pointer to the new object
 *
 *              Failure:        NULL, db_errno set
 *
 * Programmer:  matzke@viper
 *              Wed Nov  2 14:43:05 PST 1994
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:09:15 PDT 1995
 *     Made "me" static.
 *
 *     Sean Ahern, Wed Jun 14 17:19:34 PDT 2000
 *     Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBcompoundarray *
db_pdb_GetCompoundarray (DBfile *_dbfile, char *array_name)
{
   char *type = NULL;
   int            i;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   DBcompoundarray *ca = NULL;
   char          *s, delim[2], *name_vector = NULL;
   static char   *me = "db_pdb_GetCompoundarray";
   PJcomplist     tmp_obj;
   DBcompoundarray tmpca;
   memset(&tmpca, 0, sizeof(DBcompoundarray));

   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);
   DEFINE_OBJ("nelems", &tmpca.nelems, DB_INT);
   DEFINE_OBJ("nvalues", &tmpca.nvalues, DB_INT);
   DEFINE_OBJ("datatype", &tmpca.datatype, DB_INT);
   DEFALL_OBJ("elemnames", &name_vector, DB_CHAR);
   DEFALL_OBJ("elemlengths", &tmpca.elemlengths, DB_INT);
   if (PJ_GetObject(dbfile->pdb, array_name, &tmp_obj, &type) < 0)
       return NULL;
   if (NULL == (ca = DBAllocCompoundarray()))
      return NULL;
   *ca = tmpca;

   CHECK_TYPE(type, DB_ARRAY, array_name);
   if (ca->nelems <= 0 || ca->nvalues <= 0 || ca->datatype < 0 ||
       !name_vector) {
      DBFreeCompoundarray(ca);
      db_perror(array_name, E_NOTFOUND, me);
      return NULL;
   }

   /*
    *  Internally, the meshnames are stored in a single character
    *  string as a delimited set of names. Here we break them into
    *  separate names.  The delimiter is the first character of the
    *  name vector.
    */
   if (name_vector && ca->nelems > 0) {
      ca->elemnames = ALLOC_N(char *, ca->nelems);

      delim[0] = name_vector[0];
      delim[1] = '\0';
      for (i = 0; i < ca->nelems; i++) {
         s = strtok(i ? NULL : (name_vector + 1), delim);
         ca->elemnames[i] = STRDUP(s);
      }
      FREE(name_vector);
   }

   /*
    * Read the rest of the object--the vector of values, per datatype.
    */
   INIT_OBJ(&tmp_obj);
   if (DB_DOUBLE == ca->datatype && PJ_InqForceSingle()) {
      ca->datatype = DB_FLOAT;
   }

   DEFALL_OBJ("values", &ca->values, ca->datatype);
   PJ_GetObject(dbfile->pdb, array_name, &tmp_obj, NULL);

   ca->id = 0;
   ca->name = STRDUP(array_name);

   return ca;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetCurve
 *
 * Purpose:     Read a curve object from a PDB data file.
 *
 * Return:      Success:        pointer to the new object
 *
 *              Failure:        NULL, db_errno set
 *
 * Programmer:  Robb Matzke
 *              robb@callisto.nuance.com
 *              May 16, 1996
 *
 * Modifications:
 *
 *      Sean Ahern, Wed Jun 14 17:19:43 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *
 *      Thomas R. Treadway, Fri Jul  7 11:43:41 PDT 2006
 *      Added DBOPT_REFERENCE support.
 *-------------------------------------------------------------------------*/
CALLBACK DBcurve *
db_pdb_GetCurve (DBfile *_dbfile, char *name)
{
   char *type = NULL;
   DBfile_pdb   *dbfile = (DBfile_pdb *) _dbfile ;
   DBcurve      *cu ;
   static char  *me = "db_pdb_GetCurve" ;
   PJcomplist   tmp_obj ;
   DBcurve tmpcu;
   memset(&tmpcu, 0, sizeof(DBcurve));

   INIT_OBJ (&tmp_obj) ;
   DEFINE_OBJ ("npts", &tmpcu.npts, DB_INT) ;
   DEFINE_OBJ ("datatype", &tmpcu.datatype, DB_INT) ;
   DEFALL_OBJ ("label",    &tmpcu.title,    DB_CHAR) ;
   DEFALL_OBJ ("xvarname", &tmpcu.xvarname, DB_CHAR) ;
   DEFALL_OBJ ("yvarname", &tmpcu.yvarname, DB_CHAR) ;
   DEFALL_OBJ ("xlabel",   &tmpcu.xlabel,   DB_CHAR) ;
   DEFALL_OBJ ("ylabel",   &tmpcu.ylabel,   DB_CHAR) ;
   DEFALL_OBJ ("xunits",   &tmpcu.xunits,   DB_CHAR) ;
   DEFALL_OBJ ("yunits",   &tmpcu.yunits,   DB_CHAR) ;
   DEFALL_OBJ ("reference",&tmpcu.reference,DB_CHAR) ;
   DEFINE_OBJ ("guihide",  &tmpcu.guihide,  DB_INT) ;
   if (PJ_GetObject (dbfile->pdb, name, &tmp_obj, &type)<0)
       return NULL ;
   if (NULL == (cu = DBAllocCurve ())) return NULL ;
   *cu = tmpcu;

   CHECK_TYPE(type, DB_CURVE, name);
   if (cu->npts<=0)
   {
      DBFreeCurve (cu) ;
      db_perror (name, E_NOTFOUND, me) ;
      return NULL ;
   }

   if (DB_DOUBLE == cu->datatype && PJ_InqForceSingle())
      cu->datatype = DB_FLOAT ;

   /*
    * Read the x and y arrays.
    */
   if (SILO_Globals.dataReadMask & DBCurveArrays)
   {
      if (cu->reference && (cu->x || cu->y)) {
         db_perror ("x and y not NULL", E_BADARGS, me) ;
         return NULL ;
      } else if (cu->reference) {
         cu->x = NULL;
         cu->y = NULL;
      } else {
         INIT_OBJ (&tmp_obj) ;
         DEFALL_OBJ ("xvals", &cu->x, cu->datatype) ;
         DEFALL_OBJ ("yvals", &cu->y, cu->datatype) ;
         PJ_GetObject (dbfile->pdb, name, &tmp_obj, NULL) ;
      }
   }

   cu->id = 0 ;

   return cu ;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetComponent
 *
 * Purpose:     Read a component value from the data file.
 *
 * Return:      Success:        pointer to component.
 *
 *              Failure:        NULL
 *
 * Programmer:  matzke@viper
 *              Tue Nov  8 08:26:55 PST 1994
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:10:32 PDT 1995
 *     Made "me" static.
 *-------------------------------------------------------------------------*/
CALLBACK void *
db_pdb_GetComponent (DBfile *_dbfile, char *objname, char *compname)
{
   void          *result;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_GetComponent";

   result = PJ_GetComponent(dbfile->pdb, objname, compname);
   if (!result) {
      db_perror("PJ_GetComponent", E_CALLFAIL, me);
      return NULL;
   }

   return result;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetComponentType
 *
 * Purpose:     Read a component type from the data file.
 *
 * Return:      Success:        integer representing component type.
 *
 *              Failure:        DB_NOTYPE
 *
 * Programmer:  Brad Whitlock
 *              Thu Jan 20 12:02:29 PDT 2000
 *
 * Modifications:
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetComponentType (DBfile *_dbfile, char *objname, char *compname)
{
   int           retval = DB_NOTYPE;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;

   retval = PJ_GetComponentType(dbfile->pdb, objname, compname);
   return retval;
}

/*----------------------------------------------------------------------
 *  Routine                                         db_pdb_GetDefvars
 *
 *  Purpose
 *
 *      Read a defvars structure from the given database.
 *
 *  Programmger
 *
 *      Mark C. Miller,
 *      August 8, 2005
 *--------------------------------------------------------------------*/
CALLBACK DBdefvars *
db_pdb_GetDefvars(DBfile *_dbfile, const char *objname)
{
   char *typestring = NULL;
   DBdefvars     *defv = NULL;
   int            ncomps, type;
   char          *tmpnames, *tmpdefns, tmp[256];
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   static char   *me = "db_pdb_GetDefvars";

   db_pdb_getobjinfo(dbfile->pdb, (char *) objname, tmp, &ncomps);
   type = DBGetObjtypeTag(tmp);

   /* Read multi-block object */
   if (type == DB_DEFVARS)
   {
       DBdefvars tmpdefv;
       memset(&tmpdefv, 0, sizeof(DBdefvars));

       INIT_OBJ(&tmp_obj);
       DEFINE_OBJ("ndefs", &tmpdefv.ndefs, DB_INT);
       DEFALL_OBJ("types", &tmpdefv.types, DB_INT);
       DEFALL_OBJ("guihides", &tmpdefv.guihides, DB_INT);
       DEFALL_OBJ("names", &tmpnames, DB_CHAR);
       DEFALL_OBJ("defns", &tmpdefns, DB_CHAR);

       if (PJ_GetObject(dbfile->pdb, (char *) objname, &tmp_obj, &typestring) < 0)
           return NULL;
       if ((defv = DBAllocDefvars(0)) == NULL)
           return NULL;
       *defv = tmpdefv;

      CHECK_TYPE(typestring, DB_DEFVARS, objname);

       if ((tmpnames != NULL) && (defv->ndefs > 0))
       {
           defv->names = db_StringListToStringArray(tmpnames, defv->ndefs);
           FREE(tmpnames);
       }

       if ((tmpdefns != NULL) && (defv->ndefs > 0))
       {
           defv->defns = db_StringListToStringArray(tmpdefns, defv->ndefs);
           FREE(tmpdefns);
       }
   }

   return (defv);
}

/*-------------------------------------------------------------------------
 * Function:    pdb_getvarinfo
 *
 * Purpose:
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *      Sean Ahern, Wed Apr 12 11:14:38 PDT 2000
 *      Removed the last two parameters to PJ_inquire_entry because they
 *      weren't being used.
 *-------------------------------------------------------------------------*/
INTERNAL int
pdb_getvarinfo (PDBfile *pdbfile,
                char    *name,    /*Name of variable to inquire about */
                char    *type,    /*Returned datatype of 'name' */
                int     *num,     /*Returned number of elements */
                int     *size,    /*Returned element size */
                int     verbose)  /*Sentinel: 1==print error msgs; 0==quiet*/
{
   int            is_ptr;
   char           lastchar;
   char          *s;
   defstr        *dp;
   syment        *ep;

   *num = *size = 0;
   if (type)
      type[0] = '\0';

   /*
    *  Get symbol table entry for requested variable.
    */
   ep = PJ_inquire_entry(pdbfile, name);
   if (ep == NULL)
      return (OOPS);

   /* Assign values */
   if (type)
      strcpy(type, ep->type);

   lastchar = STR_LASTCHAR(ep->type);
   if (lastchar == '*')
      is_ptr = 1;
   else
      is_ptr = 0;

   if (is_ptr) {
      s = STRDUP(ep->type);
      s[strcspn(s, " *")] = '\0';

      /* Get data size of primitive (non-pointer) unit */
      dp = PD_inquire_host_type(pdbfile, s);
      *size = dp->size;
      *num = -1;              /* lite_SC_arrlen(values) / dp->size; */
      if (verbose)
         printf("Cannot query length of pointered variable.\n");

      FREE(s);

   }
   else {
      dp = PD_inquire_host_type(pdbfile, ep->type);
      if (dp == NULL) {
         if (verbose)
            printf("Don't know about data of type: %s\n", ep->type);
         return (OOPS);
      }

      /* Assign values */
      *size = dp->size;
      *num = ep->number;
   }

   return (OKAY);
}

/*----------------------------------------------------------------------
 *  Routine                                         db_pdb_GetMultimesh
 *
 *  Purpose
 *
 *      Read a multi-block-mesh structure from the given database. If the
 *      requested object is not multi-block, return the information for
 *      that one mesh only.
 *
 * Modifications
 *    Al Leibee, Wed Jul  7 08:00:00 PDT 1993
 *    Change SCALLOC_N to ALLOC_N and SC_strsave to STR_SAVE to be less
 *    SCORE dependent.  Change FREE to SCFREE to be consistant with
 *    allocation.
 *
 *    Robb Matzke, Mon Nov 14 14:37:33 EST 1994
 *    Device independence rewrite.
 *
 *    Robb Matzke, Fri Dec 2 14:11:57 PST 1994
 *    Removed references to SCORE memory management: `tmpnames'
 *
 *    Eric Brugger, Fri Jan 27 09:33:44 PST 1995
 *    I modified the routine to return NULL unless the variable is
 *    a multimesh.
 *
 *    Sean Ahern, Sun Oct  1 03:11:55 PDT 1995
 *    Made "me" static.
 *
 *    Eric Brugger, Wed Jul  2 13:30:05 PDT 1997
 *    Modify the call to DBAllocMultimesh to not allocate any arrays
 *    in the multimesh structure.
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Jeremy Meredith, Fri Jul 23 09:08:09 PDT 1999
 *    Added a check to stop strtok from occurring past the end of the
 *    array.  This was crashing the code intermittently.
 *
 *    Sean Ahern, Wed Jun 14 17:19:47 PDT 2000
 *    Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *
 *    Thomas R. Treadway, Thu Jul 20 13:34:57 PDT 2006
 *    Added lgroupings, groupings, and groupnames options.
 *
 *--------------------------------------------------------------------*/
CALLBACK DBmultimesh *
db_pdb_GetMultimesh (DBfile *_dbfile, char *objname)
{
   char *typestring = NULL;
   DBmultimesh   *mm = NULL;
   int            ncomps, type;
   int            i;
   char          *tmpnames, delim[2], *s, *name, tmp[256];
   char          *tmpgnames = NULL;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   static char   *me = "db_pdb_GetMultimesh";
   char           error[256];

   db_pdb_getobjinfo(dbfile->pdb, objname, tmp, &ncomps);
   type = DBGetObjtypeTag(tmp);

   if (type == DB_MULTIMESH) {

      DBmultimesh tmpmm;
      memset(&tmpmm, 0, sizeof(DBmultimesh));

      /* Read multi-block object */
      INIT_OBJ(&tmp_obj);
      DEFINE_OBJ("nblocks", &tmpmm.nblocks, DB_INT);
      DEFINE_OBJ("ngroups", &tmpmm.ngroups, DB_INT);
      DEFINE_OBJ("blockorigin", &tmpmm.blockorigin, DB_INT);
      DEFINE_OBJ("grouporigin", &tmpmm.grouporigin, DB_INT);
      DEFINE_OBJ("guihide", &tmpmm.guihide, DB_INT);
      DEFALL_OBJ("meshids", &tmpmm.meshids, DB_INT);
      DEFALL_OBJ("meshtypes", &tmpmm.meshtypes, DB_INT);
      DEFALL_OBJ("meshnames", &tmpnames, DB_CHAR);
      DEFALL_OBJ("meshdirs", &tmpmm.dirids, DB_INT);
      DEFINE_OBJ("extentssize", &tmpmm.extentssize, DB_INT);
      DEFALL_OBJ("extents", &tmpmm.extents, DB_DOUBLE);
      DEFALL_OBJ("zonecounts", &tmpmm.zonecounts, DB_INT);
      DEFALL_OBJ("has_external_zones", &tmpmm.has_external_zones, DB_INT);
      DEFINE_OBJ("lgroupings", &tmpmm.lgroupings, DB_INT);
      DEFALL_OBJ("groupings", &tmpmm.groupings, DB_INT);
      DEFALL_OBJ("groupnames", &tmpgnames, DB_CHAR);

      if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &typestring) < 0)
         return NULL;
      if ((mm = DBAllocMultimesh(0)) == NULL)
         return NULL;
      *mm = tmpmm;

      CHECK_TYPE(typestring, DB_MULTIMESH, objname);

      /*----------------------------------------
       *  Internally, the meshnames and groupings 
       *  iare stored in a single character string 
       *  as a delimited set of names. Here we break
       *  them into separate names.
       *----------------------------------------*/

      if ((tmpnames != NULL) && (mm->nblocks > 0)) {
         mm->meshnames = ALLOC_N(char *, mm->nblocks);

         delim[0] = tmpnames[0];
         delim[1] = '\0';
         s = &tmpnames[1];
         name = (char *)strtok(s, delim);

         for (i = 0; i < mm->nblocks; i++) {
            mm->meshnames[i] = STRDUP(name);
            if (i+1 < mm->nblocks)
            {
                name = (char *)strtok(NULL, ";");
                if (name==NULL)
                {
                    sprintf(error,"(%s) Not enough mesh names found\n",me);
                    db_perror(error, E_INTERNAL, me);
                }
            }
         }
         FREE(tmpnames);
      }
      if ((tmpgnames != NULL) && (mm->lgroupings > 0)) {
         mm->groupnames = db_StringListToStringArray(tmpgnames, mm->lgroupings);
         FREE(tmpgnames);
      }
   }

   return (mm);
}

/*----------------------------------------------------------------------
 *  Routine                                      db_pdb_GetMultimeshadj
 *
 *  Purpose
 *
 *      Read some or all of a multi-block-mesh adjacency object from
 *      the given database.
 *--------------------------------------------------------------------*/
CALLBACK DBmultimeshadj *
db_pdb_GetMultimeshadj (DBfile *_dbfile, const char *objname, int nmesh,
    const int *block_map)
{
   char *typestring = NULL;
   DBmultimeshadj   *mmadj = NULL;
   int            ncomps, type;
   int            i, j, tmpnmesh;
   char          tmp[256], *nlsname = 0, zlsname = 0;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   static char   *me = "db_pdb_GetMultimeshadj";
   char           tmpn[256];
   int           *offsetmap, *offsetmapn=0, *offsetmapz=0, lneighbors, tmpoff;

   db_pdb_getobjinfo(dbfile->pdb, (char*)objname, tmp, &ncomps);
   type = DBGetObjtypeTag(tmp);

   if (type == DB_MULTIMESHADJ) {

      DBmultimeshadj tmpmmadj;
      memset(&tmpmmadj, 0, sizeof(DBmultimeshadj));

      /* Read multi-block object */
      INIT_OBJ(&tmp_obj);
      DEFINE_OBJ("nblocks", &tmpmmadj.nblocks, DB_INT);
      DEFINE_OBJ("lneighbors", &tmpmmadj.lneighbors, DB_INT);
      DEFINE_OBJ("totlnodelists", &tmpmmadj.totlnodelists, DB_INT);
      DEFINE_OBJ("totlzonelists", &tmpmmadj.totlzonelists, DB_INT);
      DEFINE_OBJ("blockorigin", &tmpmmadj.blockorigin, DB_INT);
      DEFALL_OBJ("meshtypes", &tmpmmadj.meshtypes, DB_INT);
      DEFALL_OBJ("nneighbors", &tmpmmadj.nneighbors, DB_INT);
      DEFALL_OBJ("neighbors", &tmpmmadj.neighbors, DB_INT);
      DEFALL_OBJ("back", &tmpmmadj.back, DB_INT);
      DEFALL_OBJ("lnodelists", &tmpmmadj.lnodelists, DB_INT);
      DEFALL_OBJ("lzonelists", &tmpmmadj.lzonelists, DB_INT);

      if (PJ_GetObject(dbfile->pdb, (char*)objname, &tmp_obj, &typestring) < 0)
         return NULL;
      if ((mmadj = DBAllocMultimeshadj(0)) == NULL)
         return NULL;
      *mmadj = tmpmmadj;

      CHECK_TYPE(typestring, DB_MULTIMESHADJ, objname);

       offsetmap = ALLOC_N(int, mmadj->nblocks);
       lneighbors = 0;
       for (i = 0; i < mmadj->nblocks; i++)
       {
           offsetmap[i] = lneighbors;
           lneighbors += mmadj->nneighbors[i];
       }

       if (mmadj->lnodelists && (SILO_Globals.dataReadMask & DBMMADJNodelists))
       {
          mmadj->nodelists = ALLOC_N(int *, lneighbors); 
          offsetmapn = ALLOC_N(int, mmadj->nblocks);
          tmpoff = 0;
          for (i = 0; i < mmadj->nblocks; i++)
          {
              offsetmapn[i] = tmpoff;
              for (j = 0; j < mmadj->nneighbors[i]; j++)
                 tmpoff += mmadj->lnodelists[offsetmap[i]+j];
          }
       }

       if (mmadj->lzonelists && (SILO_Globals.dataReadMask & DBMMADJZonelists))
       {
          mmadj->zonelists = ALLOC_N(int *, lneighbors); 
          offsetmapz = ALLOC_N(int, mmadj->nblocks);
          tmpoff = 0;
          for (i = 0; i < mmadj->nblocks; i++)
          {
              offsetmapz[i] = tmpoff;
              for (j = 0; j < mmadj->nneighbors[i]; j++)
                 tmpoff += mmadj->lzonelists[offsetmap[i]+j];
          }
       }
       
       tmpnmesh = nmesh;
       if (nmesh <= 0 || !block_map)
           tmpnmesh = mmadj->nblocks;

       /* This loop could be optimized w.r.t. number of I/O requests
          it makes. The nodelists and/or zonelists could be read in
          a single call. But then we'd have to split it into separate
          arrays duplicating memory */
       for (i = 0; (i < tmpnmesh) &&
                   (SILO_Globals.dataReadMask & (DBMMADJNodelists|DBMMADJZonelists)); i++)
       {
          int blockno = block_map ? block_map[i] : i;

          if (mmadj->lnodelists && (SILO_Globals.dataReadMask & DBMMADJNodelists))
          {
             tmpoff = offsetmapn[blockno];
             for (j = 0; j < mmadj->nneighbors[blockno]; j++)
             {
                long ind[3];
                int len = mmadj->lnodelists[offsetmap[blockno]+j];
                int *nlist = ALLOC_N(int, len);

                ind[0] = tmpoff;
                ind[1] = tmpoff + len - 1;
                ind[2] = 1;
                db_mkname(dbfile->pdb, (char*)objname, "nodelists", tmpn);
                if (!PJ_read_alt(dbfile->pdb, tmpn, nlist, ind)) {
                   FREE(offsetmap);
                   FREE(offsetmapn);
                   FREE(offsetmapz);
                   db_perror("PJ_read_alt", E_CALLFAIL, me);
                }

                mmadj->nodelists[offsetmap[blockno]+j] = nlist;
                tmpoff += len;
             }
          }

          if (mmadj->lzonelists && (SILO_Globals.dataReadMask & DBMMADJZonelists))
          {
             tmpoff = offsetmapz[blockno];
             for (j = 0; j < mmadj->nneighbors[blockno]; j++)
             {
                long ind[3];
                int len = mmadj->lzonelists[offsetmap[blockno]+j];
                int *zlist = ALLOC_N(int, len);

                ind[0] = tmpoff;
                ind[1] = tmpoff + len - 1;
                ind[2] = 1;
                db_mkname(dbfile->pdb, (char*)objname, "zonelists", tmpn);
                if (!PJ_read_alt(dbfile->pdb, tmpn, zlist, ind)) {
                   FREE(offsetmap);
                   FREE(offsetmapn);
                   FREE(offsetmapz);
                   db_perror("PJ_read_alt", E_CALLFAIL, me);
                }

                mmadj->zonelists[offsetmap[blockno]+j] = zlist;
                tmpoff += len;
             }
          }
       }

       FREE(offsetmap);
       FREE(offsetmapn);
       FREE(offsetmapz);
   }

   return (mmadj);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetMultivar
 *
 * Purpose:     Read a multi-block-var structure from the given database.
 *
 * Return:      Success:        ptr to a new structure
 *
 *              Failure:        NULL
 *
 * Programmer:  robb@cloud
 *              Tue Feb 21 11:04:39 EST 1995
 *
 * Modifications:
 *    Sean Ahern, Sun Oct  1 03:12:16 PDT 1995
 *    Made "me" static.
 *
 *    Eric Brugger, Wed Jul  2 13:30:05 PDT 1997
 *    Modify the call to DBAllocMultivar to not allocate any arrays
 *    in the multivar structure.
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Jeremy Meredith, Fri Jul 23 09:08:09 PDT 1999
 *    Added a check to stop strtok from occurring past the end of the
 *    array.  This was crashing the code intermittently.
 *
 *    Sean Ahern, Wed Jun 14 17:20:24 PDT 2000
 *    Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBmultivar *
db_pdb_GetMultivar (DBfile *_dbfile, char *objname)
{
   char *typestring = NULL;
   DBmultivar    *mv = NULL;
   int            ncomps, type;
   int            i;
   char          *tmpnames, delim[2], *s, *name, tmp[256];
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   static char   *me = "db_pdb_GetMultivar";
   char           error[256];

   db_pdb_getobjinfo(dbfile->pdb, objname, tmp, &ncomps);
   type = DBGetObjtypeTag(tmp);

   if (type == DB_MULTIVAR) {

      DBmultivar tmpmv;
      memset(&tmpmv, 0, sizeof(DBmultivar));

      /* Read multi-block object */
      INIT_OBJ(&tmp_obj);
      DEFINE_OBJ("nvars", &tmpmv.nvars, DB_INT);
      DEFALL_OBJ("vartypes", &tmpmv.vartypes, DB_INT);
      DEFALL_OBJ("varnames", &tmpnames, DB_CHAR);
      DEFINE_OBJ("ngroups", &tmpmv.ngroups, DB_INT);
      DEFINE_OBJ("blockorigin", &tmpmv.blockorigin, DB_INT);
      DEFINE_OBJ("grouporigin", &tmpmv.grouporigin, DB_INT);
      DEFINE_OBJ("extentssize", &tmpmv.extentssize, DB_INT);
      DEFALL_OBJ("extents", &tmpmv.extents, DB_DOUBLE);
      DEFINE_OBJ("guihide", &tmpmv.guihide, DB_INT);

      if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &typestring) < 0)
         return NULL;
      if ((mv = DBAllocMultivar(0)) == NULL)
         return NULL;
      *mv = tmpmv;
      CHECK_TYPE(typestring, DB_MULTIVAR, objname);

      /*----------------------------------------
       *  Internally, the varnames are stored
       *  in a single character string as a
       *  delimited set of names. Here we break
       *  them into separate names.
       *----------------------------------------*/

      if (tmpnames != NULL && mv->nvars > 0) {
         mv->varnames = ALLOC_N(char *, mv->nvars);

         delim[0] = tmpnames[0];
         delim[1] = '\0';
         s = &tmpnames[1];
         name = (char *)strtok(s, delim);

         for (i = 0; i < mv->nvars; i++) {
            mv->varnames[i] = STRDUP(name);
            if (i+1 < mv->nvars)
            {
                name = (char *)strtok(NULL, ";");
                if (name==NULL)
                {
                    sprintf(error,"(%s) Not enough variable names found\n",me);
                    db_perror(error, E_INTERNAL, me);
                }
            }
         }
         FREE(tmpnames);
      }
   }

   return (mv);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetMultimat
 *
 * Purpose:     Read a multi-material structure from the given database.
 *
 * Return:      Success:        ptr to a new structure
 *
 *              Failure:        NULL
 *
 * Programmer:  robb@cloud
 *              Tue Feb 21 12:39:51 EST 1995
 *
 * Modifications:
 *    Sean Ahern, Sun Oct  1 03:12:39 PDT 1995
 *    Made "me" static.
 *
 *    Sean Ahern, Mon Jun 24 14:44:44 PDT 1996
 *    Added better error checking to keep strtok from running off the
 *    end of the tmpnames array,
 *
 *    Sean Ahern, Mon Jun 24 15:03:43 PDT 1996
 *    Fixed a memory leak where mt->matnames was being allocated twice,
 *    once in DBAllocMultimat, and once here.
 *
 *    Sean Ahern, Tue Jun 25 13:41:23 PDT 1996
 *    Since we know that material names are delimited by ';', we don't
 *    need the delim array.  Just use ";" instead.
 *
 *    Eric Brugger, Wed Jul  2 13:30:05 PDT 1997
 *    Modify the call to DBAllocMultimat to not allocate any arrays
 *    in the multimat structure.
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Sean Ahern, Wed Jun 14 17:20:55 PDT 2000
 *    Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBmultimat *
db_pdb_GetMultimat (DBfile *_dbfile, char *objname)
{
   char *typestring = NULL;
   DBmultimat    *mt = NULL;
   int            ncomps, type;
   int            i;
   char          *tmpnames=NULL, *s=NULL, *name=NULL, tmp[256];
   char           error[256];
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   static char   *me = "db_pdb_GetMultimat";

   db_pdb_getobjinfo(dbfile->pdb, objname, tmp, &ncomps);
   type = DBGetObjtypeTag(tmp);

   if (type == DB_MULTIMAT) {

      DBmultimat tmpmt;
      memset(&tmpmt, 0, sizeof(DBmultimat));


      /* Read multi-block object */
      INIT_OBJ(&tmp_obj);
      DEFINE_OBJ("nmats", &tmpmt.nmats, DB_INT);
      DEFALL_OBJ("matnames", &tmpnames, DB_CHAR);
      DEFINE_OBJ("ngroups", &tmpmt.ngroups, DB_INT);
      DEFINE_OBJ("blockorigin", &tmpmt.blockorigin, DB_INT);
      DEFINE_OBJ("grouporigin", &tmpmt.grouporigin, DB_INT);
      DEFALL_OBJ("mixlens", &tmpmt.mixlens, DB_INT);
      DEFALL_OBJ("matcounts", &tmpmt.matcounts, DB_INT);
      DEFALL_OBJ("matlists", &tmpmt.matlists, DB_INT);
      DEFINE_OBJ("guihide", &tmpmt.guihide, DB_INT);

      if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &typestring) < 0)
         return NULL;
      if ((mt = DBAllocMultimat(0)) == NULL)
         return NULL;
      *mt = tmpmt;
      CHECK_TYPE(typestring, DB_MULTIMAT, objname);

      /*----------------------------------------
       *  Internally, the material names are stored
       *  in a single character string as a
       *  delimited set of names. Here we break
       *  them into separate names.
       *----------------------------------------*/

      if (tmpnames != NULL && mt->nmats > 0)
      {
         FREE(mt->matnames);
         mt->matnames = ALLOC_N(char *, mt->nmats);

         s = &tmpnames[1];
            name = (char *)strtok(s, ";");

         for (i = 0; i < mt->nmats; i++)
         {
            mt->matnames[i] = STRDUP(name);

            if (i+1 < mt->nmats)
            {
                name = (char *)strtok(NULL, ";");
                if (name==NULL)
                {
                    sprintf(error,"(%s) Not enough material names found\n",me);
                    db_perror(error, E_INTERNAL, me);
                }
            }
         }
         FREE(tmpnames);
      }
   }

   return (mt);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetMultimatspecies
 *
 * Purpose:     Read a multi-species structure from the given database.
 *
 * Return:      Success:        ptr to a new structure
 *
 *              Failure:        NULL
 *
 * Programmer:  Jeremy S. Meredith
 *              Sept 17 1998
 *
 * Modifications:
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Sean Ahern, Wed Jun 14 17:21:18 PDT 2000
 *    Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBmultimatspecies *
db_pdb_GetMultimatspecies (DBfile *_dbfile, char *objname)
{
   char *typestring = NULL;
   DBmultimatspecies *mms = NULL;
   int                ncomps, type;
   int                i;
   char              *tmpnames=NULL, *s=NULL, *name=NULL, tmp[256];
   char               error[256];
   DBfile_pdb        *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist         tmp_obj;
   static char       *me = "db_pdb_GetMultimatspecies";

   db_pdb_getobjinfo(dbfile->pdb, objname, tmp, &ncomps);
   type = DBGetObjtypeTag(tmp);

   if (type == DB_MULTIMATSPECIES) {

      DBmultimatspecies tmpmms;
      memset(&tmpmms, 0, sizeof(DBmultimatspecies));


      /* Read multi-block object */
      INIT_OBJ(&tmp_obj);
      DEFINE_OBJ("nspec", &tmpmms.nspec, DB_INT);
      DEFALL_OBJ("specnames", &tmpnames, DB_CHAR);
      DEFINE_OBJ("ngroups", &tmpmms.ngroups, DB_INT);
      DEFINE_OBJ("blockorigin", &tmpmms.blockorigin, DB_INT);
      DEFINE_OBJ("grouporigin", &tmpmms.grouporigin, DB_INT);
      DEFINE_OBJ("guihide", &tmpmms.guihide, DB_INT);

      if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &typestring) < 0)
         return NULL;
      if ((mms = DBAllocMultimatspecies(0)) == NULL)
         return NULL;
      *mms = tmpmms;
      CHECK_TYPE(typestring, DB_MULTIMATSPECIES, objname);

      /*----------------------------------------
       *  Internally, the material species names
       *  are stored in a single character string
       *  as a delimited set of names. Here we
       *  break them into separate names.
       *----------------------------------------*/

      if (tmpnames != NULL && mms->nspec > 0)
      {
         FREE(mms->specnames);
         mms->specnames = ALLOC_N(char *, mms->nspec);

         s = &tmpnames[1];
            name = (char *)strtok(s, ";");

         for (i = 0; i < mms->nspec; i++)
         {
            mms->specnames[i] = STRDUP(name);

            if (i+1 < mms->nspec)
            {
                name = (char *)strtok(NULL, ";");
                if (name==NULL)
                {
                    sprintf(error,"(%s) Not enough species names found\n",me);
                    db_perror(error, E_INTERNAL, me);
                }
            }
         }
         FREE(tmpnames);
      }
   }

   return (mms);
}

/*----------------------------------------------------------------------
 *  Routine                                         db_pdb_GetPointmesh
 *
 *  Purpose
 *
 *      Read a point-mesh object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modifications
 *
 *      Al Leibee, Tue Apr 19 08:56:11 PDT 1994
 *      Added dtime.
 *
 *      Robb Matzke, Mon Nov 14 15:49:15 EST 1994
 *      Device independence rewrite
 *
 *      Robb Matzke, 27 Aug 1997
 *      The datatype is changed to DB_FLOAT only if it was DB_DOUBLE
 *      and force_single is turned on.
 *
 *      Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *      Added group_no.
 *
 *      Sean Ahern, Wed Jun 14 17:21:36 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *--------------------------------------------------------------------*/
CALLBACK DBpointmesh *
db_pdb_GetPointmesh (DBfile *_dbfile, char *objname)
{
   char *type = NULL;
   DBpointmesh   *pm = NULL;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   static char *me = "db_pdb_GetPointmesh";
   DBpointmesh tmppm;
   memset(&tmppm, 0, sizeof(DBpointmesh));


   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   DEFINE_OBJ("block_no", &tmppm.block_no, DB_INT);
   DEFINE_OBJ("group_no", &tmppm.group_no, DB_INT);
   DEFINE_OBJ("cycle", &tmppm.cycle, DB_INT);
   DEFINE_OBJ("time", &tmppm.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmppm.dtime, DB_DOUBLE);
   DEFINE_OBJ("datatype", &tmppm.datatype, DB_INT);
   DEFINE_OBJ("ndims", &tmppm.ndims, DB_INT);
   DEFINE_OBJ("nels", &tmppm.nels, DB_INT);
   DEFINE_OBJ("origin", &tmppm.origin, DB_INT);

   DEFINE_OBJ("min_extents", tmppm.min_extents, DB_FLOAT);
   DEFINE_OBJ("max_extents", tmppm.max_extents, DB_FLOAT);

   DEFINE_OBJ("guihide", &tmppm.guihide, DB_INT);

   if (SILO_Globals.dataReadMask & DBPMCoords)
   {
       DEFALL_OBJ("coord0", &tmppm.coords[0], DB_FLOAT);
       DEFALL_OBJ("coord1", &tmppm.coords[1], DB_FLOAT);
       DEFALL_OBJ("coord2", &tmppm.coords[2], DB_FLOAT);
   }

   if (SILO_Globals.dataReadMask & DBPMGlobNodeNo)
       DEFALL_OBJ("gnodeno", &tmppm.gnodeno, DB_INT);

   DEFALL_OBJ("label0", &tmppm.labels[0], DB_CHAR);
   DEFALL_OBJ("label1", &tmppm.labels[1], DB_CHAR);
   DEFALL_OBJ("label2", &tmppm.labels[2], DB_CHAR);
   DEFALL_OBJ("units0", &tmppm.units[0], DB_CHAR);
   DEFALL_OBJ("units1", &tmppm.units[1], DB_CHAR);
   DEFALL_OBJ("units2", &tmppm.units[2], DB_CHAR);

   if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
      return NULL;
   if ((pm = DBAllocPointmesh()) == NULL)
      return NULL;
   *pm = tmppm;
   CHECK_TYPE(type, DB_POINTMESH, objname);

   pm->id = 0;
   pm->name = STRDUP(objname);
   if (DB_DOUBLE==pm->datatype && PJ_InqForceSingle()) {
      pm->datatype = DB_FLOAT;
   }

   return (pm);
}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_GetPointvar
 *
 *  Purpose
 *
 *      Read a point-var object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modifications
 *
 *      Al Leibee, Tue Apr 19 08:56:11 PDT 1994
 *      Added dtime.
 *
 *      Robb Matzke, Mon Nov 14 15:58:36 EST 1994
 *      Device independence rewrite.
 *
 *      Sean Ahern, Fri May 24 17:35:58 PDT 1996
 *      Added smarts for figuring out the datatype, similar to
 *      db_pdb_GetUcdvar and db_pdb_GetQuadvar.
 *
 *      Sean Ahern, Fri May 24 17:36:31 PDT 1996
 *      I corrected a possible bug where, if force single were on,
 *      the datatype would only be set to DB_FLOAT if the datatype
 *      had previously been DB_DOUBLE.  This ignored things like
 *      DB_INT.
 *
 *      Eric Brugger, Fri Sep  6 08:12:40 PDT 1996
 *      I removed the reading of "meshid" since it is not used and
 *      is actually stored as a string in the silo files not an
 *      integer.  This caused a memory overwrite which only hurt
 *      things if the string was more than 104 characters.
 *
 *      Sam Wookey, Wed Jul  1 16:21:09 PDT 1998
 *      Corrected the way we retrieve variables with nvals > 1.
 *
 *      Sean Ahern, Wed Jun 14 17:22:54 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Eric Brugger, Tue Mar  2 16:21:15 PST 2004
 *      Modified the routine to use _ptvalstr for the names of the
 *      components when the variable has more than one component.
 *      Each component name needs to be a seperate string for things
 *      to work properly, since a pointer to the string is stored and
 *      not a copy of the string.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *
 *--------------------------------------------------------------------*/
CALLBACK DBmeshvar *
db_pdb_GetPointvar (DBfile *_dbfile, char *objname)
{
   char *type = NULL;
   DBmeshvar     *mv = NULL;
   int            i;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   char           tmp[256];
   static char *me = "db_pdb_GetPointvar";
   DBmeshvar tmpmv;
   memset(&tmpmv, 0, sizeof(DBmeshvar));


   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   DEFINE_OBJ("cycle", &tmpmv.cycle, DB_INT);
   DEFINE_OBJ("time", &tmpmv.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmpmv.dtime, DB_DOUBLE);
   DEFINE_OBJ("datatype", &tmpmv.datatype, DB_INT);
   DEFINE_OBJ("ndims", &tmpmv.ndims, DB_INT);
   DEFINE_OBJ("nels", &tmpmv.nels, DB_INT);
   DEFINE_OBJ("nvals", &tmpmv.nvals, DB_INT);
   DEFINE_OBJ("origin", &tmpmv.origin, DB_INT);

   DEFALL_OBJ("label", &tmpmv.label, DB_CHAR);
   DEFALL_OBJ("units", &tmpmv.units, DB_CHAR);
   DEFALL_OBJ("meshid",&tmpmv.meshname, DB_CHAR);
   DEFINE_OBJ("guihide", &tmpmv.guihide, DB_INT);

   if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
      return NULL;
   if ((mv = DBAllocMeshvar()) == NULL)
      return NULL;
   *mv = tmpmv;
   CHECK_TYPE(type, DB_POINTVAR, objname);

   /*
    *  Read the remainder of the object: loop over all values
    *  associated with this variable.
    */

   if ((mv->nvals > 0) && (SILO_Globals.dataReadMask & DBPVData)) {
      INIT_OBJ(&tmp_obj);

      mv->vals = ALLOC_N(float *, mv->nvals);

      if (mv->datatype == 0) {
          if(mv->nvals == 1)
          {
              sprintf(tmp, "%s_data", objname);
          }
          else
          {
              sprintf(tmp, "%s_0_data", objname);
          }
          if ((mv->datatype = db_pdb_GetVarDatatype(dbfile->pdb, tmp)) < 0) {
              /* Not found. Assume float. */
              mv->datatype = DB_FLOAT;
          }
      }

      if (PJ_InqForceSingle())
          mv->datatype = DB_FLOAT;

      if(mv->nvals == 1)
      {
          DEFALL_OBJ("_data", &mv->vals[0], DB_FLOAT);
      }
      else
      {
          for (i = 0; i < mv->nvals; i++)
          {
              DEFALL_OBJ(_ptvalstr[i], &mv->vals[i], DB_FLOAT);
          }
      }

      PJ_GetObject(dbfile->pdb, objname, &tmp_obj, NULL);
   }

   mv->id = 0;
   mv->name = STRDUP(objname);

   return (mv);
}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_GetQuadmesh
 *
 *  Purpose
 *
 *      Read a quad-mesh object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modifications
 *
 *      Al Leibee, Tue Apr 19 08:56:11 PDT 1994
 *      Added dtime.
 *
 *      Robb Matzke, Mon Nov 14 16:03:55 EST 1994
 *      Device independence rewrite.
 *
 *      Sean Ahern, Fri Aug  1 13:24:28 PDT 1997
 *      Reformatted code through "indent".
 *
 *      Sean Ahern, Fri Aug  1 13:24:45 PDT 1997
 *      Changed how the datatype is computed.  It used to be unconditionally
 *      set to DB_FLOAT.  Now we only do that if we're doing a force single.
 *
 *      Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *      Added group_no and baseindex[].
 *
 *      Eric Brugger, Thu Aug 26 09:05:20 PDT 1999
 *      I modified the routine to set the base_index field base on
 *      the origin field when the base_index field is not defined in
 *      the file.
 *
 *      Sean Ahern, Wed Jun 14 17:36:42 PDT 2000
 *      Added checks to make sure the object has the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *--------------------------------------------------------------------*/
CALLBACK DBquadmesh *
db_pdb_GetQuadmesh (DBfile *_dbfile, char *objname)
{
    char *type = NULL;
    DBquadmesh     *qm = NULL;
    PJcomplist      tmp_obj;
    DBfile_pdb     *dbfile = (DBfile_pdb *) _dbfile;
    static char *me = "db_pdb_GetQuadmesh";
    DBquadmesh tmpqm;
    memset(&tmpqm, 0, sizeof(DBquadmesh));

    tmpqm.base_index[0] = -99999;

    /*------------------------------------------------------------*
     * Comp. Name        Comp. Address     Data Type              *
     *------------------------------------------------------------*/
    INIT_OBJ(&tmp_obj);

    DEFINE_OBJ("block_no", &tmpqm.block_no, DB_INT);
    DEFINE_OBJ("group_no", &tmpqm.group_no, DB_INT);
    DEFINE_OBJ("cycle", &tmpqm.cycle, DB_INT);
    DEFINE_OBJ("time", &tmpqm.time, DB_FLOAT);
    DEFINE_OBJ("dtime", &tmpqm.dtime, DB_DOUBLE);
    DEFINE_OBJ("datatype", &tmpqm.datatype, DB_INT);
    DEFINE_OBJ("coord_sys", &tmpqm.coord_sys, DB_INT);
    DEFINE_OBJ("coordtype", &tmpqm.coordtype, DB_INT);
    DEFINE_OBJ("facetype", &tmpqm.facetype, DB_INT);
    DEFINE_OBJ("planar", &tmpqm.planar, DB_INT);
    DEFINE_OBJ("ndims", &tmpqm.ndims, DB_INT);
    DEFINE_OBJ("nspace", &tmpqm.nspace, DB_INT);
    DEFINE_OBJ("nnodes", &tmpqm.nnodes, DB_INT);
    DEFINE_OBJ("major_order", &tmpqm.major_order, DB_INT);
    DEFINE_OBJ("origin", &tmpqm.origin, DB_INT);

    if (SILO_Globals.dataReadMask & DBQMCoords)
    {
        DEFALL_OBJ("coord0", &tmpqm.coords[0], DB_FLOAT);
        DEFALL_OBJ("coord1", &tmpqm.coords[1], DB_FLOAT);
        DEFALL_OBJ("coord2", &tmpqm.coords[2], DB_FLOAT);
    }
    DEFALL_OBJ("label0", &tmpqm.labels[0], DB_CHAR);
    DEFALL_OBJ("label1", &tmpqm.labels[1], DB_CHAR);
    DEFALL_OBJ("label2", &tmpqm.labels[2], DB_CHAR);
    DEFALL_OBJ("units0", &tmpqm.units[0], DB_CHAR);
    DEFALL_OBJ("units1", &tmpqm.units[1], DB_CHAR);
    DEFALL_OBJ("units2", &tmpqm.units[2], DB_CHAR);

    DEFINE_OBJ("dims", tmpqm.dims, DB_INT);
    DEFINE_OBJ("min_index", tmpqm.min_index, DB_INT);
    DEFINE_OBJ("max_index", tmpqm.max_index, DB_INT);
    DEFINE_OBJ("min_extents", tmpqm.min_extents, DB_FLOAT);
    DEFINE_OBJ("max_extents", tmpqm.max_extents, DB_FLOAT);
    DEFINE_OBJ("baseindex", tmpqm.base_index, DB_INT);
    DEFINE_OBJ("guihide", tmpqm.guihide, DB_INT);

    /* The type passed here to PJ_GetObject is NULL because quadmeshes can have
     * one of two types:  DB_COLLINEAR or DB_NONCOLLINEAR.  */
    if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
       return NULL;
    if ((qm = DBAllocQuadmesh()) == NULL)
       return NULL;
    *qm = tmpqm;
    if ((strcmp(type, DBGetObjtypeName(DB_QUAD_RECT)) != 0)
        &&
        (strcmp(type, DBGetObjtypeName(DB_QUAD_CURV)) != 0)
       )
    {
        char error[256];
        sprintf(error,"Requested %s object \"%s\" is not a quadmesh.",
                type, objname);
        FREE(type);
        db_perror(error, E_INTERNAL, me);
    } else {
        FREE(type);
    }

    /*
     * If base_index was not set with the PJ_GetObject call, then
     * set it based on origin.
     */
    if (qm->base_index[0] == -99999)
    {
        int       i;

        for (i = 0; i < qm->ndims; i++)
        {
            qm->base_index[i] = qm->origin;
        }
    }

    qm->id = 0;
    qm->name = STRDUP(objname);

    if (PJ_InqForceSingle())
        qm->datatype = DB_FLOAT;

    _DBQMSetStride(qm);

    return (qm);
}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_GetQuadvar
 *
 *  Purpose
 *
 *      Read a quad-var object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modifications
 *
 *      Al Leibee, Wed Jul 20 07:56:37 PDT 1994
 *      Added get of mixed zone data arrays.
 *
 *      Al Leibee, Tue Apr 19 08:56:11 PDT 1994
 *      Added dtime.
 *
 *      Robb Matzke, Mon Nov 14 17:02:44 EST 1994
 *      Device independence rewrite.
 *
 *      Sean Ahern, Fri Sep 29 18:39:08 PDT 1995
 *      Requested DB_FLOATS instead of qv->datatype.
 *
 *      Sean Ahern, Fri May 24 17:20:00 PDT 1996
 *      I corrected a possible bug where, if force single were on,
 *      the datatype would only be set to DB_FLOAT if the datatype
 *      had previously been DB_DOUBLE.  This ignored things like
 *      DB_INT.
 *
 *      Robb Matzke, 19 Jun 1997
 *      Reads the optional `ascii_labels' field.
 *
 *      Sean Ahern, Wed Jun 14 17:23:49 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Brad Whitlock, Wed Jul 21 12:05:56 PDT 2004
 *      I fixed a coding error that prevented PJ_GetObject from
 *      reading in labels and units for the quadvar.
 *      value of PJ_GetObject.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *--------------------------------------------------------------------*/
CALLBACK DBquadvar *
db_pdb_GetQuadvar (DBfile *_dbfile, char *objname)
{
   char *type = NULL;
   DBquadvar     *qv = NULL;
   int            i;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   char           tmp[256];
   static char *me = "db_pdb_GetQuadvar";
   DBquadvar tmpqv;
   memset(&tmpqv, 0, sizeof(DBquadvar));


   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   /* Scalars */
   DEFINE_OBJ("cycle", &tmpqv.cycle, DB_INT);
   DEFINE_OBJ("time", &tmpqv.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmpqv.dtime, DB_DOUBLE);
   DEFINE_OBJ("datatype", &tmpqv.datatype, DB_INT);
   DEFINE_OBJ("ndims", &tmpqv.ndims, DB_INT);
   DEFINE_OBJ("major_order", &tmpqv.major_order, DB_INT);
   DEFINE_OBJ("nels", &tmpqv.nels, DB_INT);
   DEFINE_OBJ("nvals", &tmpqv.nvals, DB_INT);
   DEFINE_OBJ("origin", &tmpqv.origin, DB_INT);
   DEFINE_OBJ("mixlen", &tmpqv.mixlen, DB_INT);
   DEFINE_OBJ("use_specmf", &tmpqv.use_specmf, DB_INT);
   DEFINE_OBJ("ascii_labels", &tmpqv.ascii_labels, DB_INT);
   DEFALL_OBJ("meshid", &tmpqv.meshname, DB_CHAR);
   DEFINE_OBJ("guihide", &tmpqv.guihide, DB_INT);

   /* Arrays */
   DEFINE_OBJ("min_index", tmpqv.min_index, DB_INT);
   DEFINE_OBJ("max_index", tmpqv.max_index, DB_INT);
   DEFINE_OBJ("dims", tmpqv.dims, DB_INT);
   DEFINE_OBJ("align", tmpqv.align, DB_FLOAT);

   /* Arrays that PJ_GetObject must allocate. */
   DEFALL_OBJ("label", &tmpqv.label, DB_CHAR);
   DEFALL_OBJ("units", &tmpqv.units, DB_CHAR);

   if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
      return NULL;
   if ((qv = DBAllocQuadvar()) == NULL)
      return NULL;
   *qv = tmpqv;
   CHECK_TYPE(type, DB_QUADVAR, objname);

   /*
    *  Read the remainder of the object: loop over all values
    *  associated with this variable.
    */

   if ((qv->nvals > 0) && (SILO_Globals.dataReadMask & DBQVData)) {
      INIT_OBJ(&tmp_obj);

      qv->vals = ALLOC_N(float *, qv->nvals);

      if (qv->mixlen > 0) {
         qv->mixvals = ALLOC_N(float *, qv->nvals);
      }

      if (qv->datatype == 0) {
         strcpy(tmp, objname);
         strcat(tmp, "_data");
         if ((qv->datatype = db_pdb_GetVarDatatype(dbfile->pdb, tmp)) < 0) {
            /* Not found. Assume float. */
            qv->datatype = DB_FLOAT;
         }
      }

      if (PJ_InqForceSingle())
         qv->datatype = DB_FLOAT;

      for (i = 0; i < qv->nvals; i++) {
         DEFALL_OBJ(_valstr[i], &qv->vals[i], DB_FLOAT);

         if (qv->mixlen > 0) {
            DEFALL_OBJ(_mixvalstr[i], &qv->mixvals[i], DB_FLOAT);
         }
      }

      PJ_GetObject(dbfile->pdb, objname, &tmp_obj, NULL);
   }

   qv->id = 0;
   qv->name = STRDUP(objname);

   _DBQQCalcStride(qv->stride, qv->dims, qv->ndims, qv->major_order);

   return (qv);
}

/*----------------------------------------------------------------------
 *  Routine                                                DBGetUcdmesh
 *
 *  Purpose
 *
 *      Read a ucd-mesh structure from the given database.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Parameters
 *
 *      DBGetUcdmesh  {Out}   {Pointer to DBucdmesh structure}
 *      dbfile        {In}    {Pointer to current file}
 *      meshname      {In}    {Name of ucdmesh to read}
 *
 *  Notes
 *
 *  Modified
 *
 *      Robb Matzke, Tue Nov 15 10:54:15 EST 1994
 *      Device independence rewrite.
 *
 *      Eric Brugger, Wed Dec 11 13:01:52 PST 1996
 *      I plugged some memory leaks by freeing flname, zlname and
 *      elname at the end.
 *
 *      Eric Brugger, Tue Oct 20 12:46:53 PDT 1998
 *      Modify the routine to read in the min_index and max_index
 *      fields of the zonelist.
 *
 *      Eric Brugger, Tue Mar 30 11:17:58 PST 1999
 *      Modify the routine to read in the shapetype field of the
 *      zonelist.  This field is optional.
 *
 *      Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *      Added group_no, gnodeno, and gzoneno.
 *
 *      Sean Ahern, Wed Jun 14 17:24:07 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc calls to after PJ_GetObject. Added automatic
 *      vars for PJ_GetObject to read into. Added checks for return
 *      value of PJ_GetObject calls.
 *--------------------------------------------------------------------*/
CALLBACK DBucdmesh *
db_pdb_GetUcdmesh (DBfile *_dbfile, char *meshname)
{
   char *type = NULL;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   DBucdmesh     *um = NULL;
   char          *flname = NULL, *zlname = NULL, *elname = NULL,
                 *phzlname = NULL;
   PJcomplist     tmp_obj;
   int            lo_offset, hi_offset;
   static char *me = "db_pdb_GetUcdmesh";
   DBucdmesh tmpum;
   memset(&tmpum, 0, sizeof(DBucdmesh));


   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   DEFINE_OBJ("block_no", &tmpum.block_no, DB_INT);
   DEFINE_OBJ("group_no", &tmpum.group_no, DB_INT);
   DEFINE_OBJ("cycle", &tmpum.cycle, DB_INT);
   DEFINE_OBJ("time", &tmpum.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmpum.dtime, DB_DOUBLE);
   DEFINE_OBJ("datatype", &tmpum.datatype, DB_INT);
   DEFINE_OBJ("coord_sys", &tmpum.coord_sys, DB_INT);
   DEFINE_OBJ("topo_dim", &tmpum.topo_dim, DB_INT);
   DEFINE_OBJ("ndims", &tmpum.ndims, DB_INT);
   DEFINE_OBJ("nnodes", &tmpum.nnodes, DB_INT);
   DEFINE_OBJ("origin", &tmpum.origin, DB_INT);

   DEFINE_OBJ("min_extents", tmpum.min_extents, DB_FLOAT);
   DEFINE_OBJ("max_extents", tmpum.max_extents, DB_FLOAT);

   if (SILO_Globals.dataReadMask & DBUMCoords)
   {
       DEFALL_OBJ("coord0", &tmpum.coords[0], DB_FLOAT);
       DEFALL_OBJ("coord1", &tmpum.coords[1], DB_FLOAT);
       DEFALL_OBJ("coord2", &tmpum.coords[2], DB_FLOAT);
   }
   DEFALL_OBJ("label0", &tmpum.labels[0], DB_CHAR);
   DEFALL_OBJ("label1", &tmpum.labels[1], DB_CHAR);
   DEFALL_OBJ("label2", &tmpum.labels[2], DB_CHAR);
   DEFALL_OBJ("units0", &tmpum.units[0], DB_CHAR);
   DEFALL_OBJ("units1", &tmpum.units[1], DB_CHAR);
   DEFALL_OBJ("units2", &tmpum.units[2], DB_CHAR);
   DEFINE_OBJ("guihide", &tmpum.guihide, DB_INT);

   /* Optional global node number */
   if (SILO_Globals.dataReadMask & DBUMGlobNodeNo)
       DEFALL_OBJ("gnodeno", &tmpum.gnodeno, DB_INT);

   /* Get SILO ID's for other UCD mesh components */
   DEFALL_OBJ("facelist", &flname, DB_CHAR);
   DEFALL_OBJ("zonelist", &zlname, DB_CHAR);
   DEFALL_OBJ("edgelist", &elname, DB_CHAR);

   /* Optional polyhedral zonelist */
   DEFALL_OBJ("phzonelist", &phzlname, DB_CHAR);

   if (PJ_GetObject(dbfile->pdb, meshname, &tmp_obj, &type) < 0)
      return NULL;
   if ((um = DBAllocUcdmesh()) == NULL)
      return NULL;
   *um = tmpum;
   CHECK_TYPE(type, DB_UCDMESH, meshname);

   if (PJ_InqForceSingle() == TRUE)
      um->datatype = DB_FLOAT;

   um->id = 0;
   um->name = STRDUP(meshname);

   /* Read facelist, zonelist, and edgelist */

   if ((flname != NULL && strlen(flname) > 0)
       && (SILO_Globals.dataReadMask & DBUMFacelist))
   {
      DBfacelist tmpfaces;
      memset(&tmpfaces, 0, sizeof(DBfacelist));


      /*------------------------------------------------------------*/
      /*          Comp. Name        Comp. Address     Data Type     */
      /*------------------------------------------------------------*/
      INIT_OBJ(&tmp_obj);

      DEFINE_OBJ("ndims", &tmpfaces.ndims, DB_INT);
      DEFINE_OBJ("nfaces", &tmpfaces.nfaces, DB_INT);
      DEFINE_OBJ("lnodelist", &tmpfaces.lnodelist, DB_INT);
      DEFINE_OBJ("nshapes", &tmpfaces.nshapes, DB_INT);
      DEFINE_OBJ("ntypes", &tmpfaces.ntypes, DB_INT);
      DEFINE_OBJ("origin", &tmpfaces.origin, DB_INT);

      DEFALL_OBJ("nodelist", &tmpfaces.nodelist, DB_INT);
      DEFALL_OBJ("shapesize", &tmpfaces.shapesize, DB_INT);
      DEFALL_OBJ("shapecnt", &tmpfaces.shapecnt, DB_INT);
      DEFALL_OBJ("typelist", &tmpfaces.typelist, DB_INT);
      DEFALL_OBJ("types", &tmpfaces.types, DB_INT);
      DEFALL_OBJ("zoneno", &tmpfaces.zoneno, DB_INT);

      if (PJ_GetObject(dbfile->pdb, flname, &tmp_obj, NULL) < 0)
      {
         DBFreeUcdmesh(um);
         return NULL;
      }
      if ((um->faces = DBAllocFacelist()) == NULL)
      {
         DBFreeUcdmesh(um);
         return NULL;
      }
      *(um->faces) = tmpfaces;

   }

   if ((zlname != NULL && strlen(zlname) > 0)
       && (SILO_Globals.dataReadMask & DBUMZonelist))
   {
      DBzonelist tmpzones;
      memset(&tmpzones, 0, sizeof(DBzonelist));

      /*------------------------------------------------------------*/
      /*          Comp. Name        Comp. Address     Data Type     */
      /*------------------------------------------------------------*/
      INIT_OBJ(&tmp_obj);

      DEFINE_OBJ("ndims", &tmpzones.ndims, DB_INT);
      DEFINE_OBJ("nzones", &tmpzones.nzones, DB_INT);
      DEFINE_OBJ("nshapes", &tmpzones.nshapes, DB_INT);
      DEFINE_OBJ("lnodelist", &tmpzones.lnodelist, DB_INT);
      DEFINE_OBJ("origin", &tmpzones.origin, DB_INT);

      DEFALL_OBJ("nodelist", &tmpzones.nodelist, DB_INT);
      DEFALL_OBJ("shapetype", &tmpzones.shapetype, DB_INT);
      DEFALL_OBJ("shapesize", &tmpzones.shapesize, DB_INT);
      DEFALL_OBJ("shapecnt", &tmpzones.shapecnt, DB_INT);

      /* Optional global zone number */
      if (SILO_Globals.dataReadMask & DBZonelistGlobZoneNo)
          DEFALL_OBJ("gzoneno", &tmpzones.gzoneno, DB_INT);

      /*----------------------------------------------------------*/
      /* These are optional so set them to their default values   */
      /*----------------------------------------------------------*/
      lo_offset = 0;
      hi_offset = 0;
      DEFINE_OBJ("lo_offset", &lo_offset, DB_INT);
      DEFINE_OBJ("hi_offset", &hi_offset, DB_INT);

      if (PJ_GetObject(dbfile->pdb, zlname, &tmp_obj, &type) < 0)
      {
         DBFreeUcdmesh(um);
         return NULL;
      }
      if ((um->zones = DBAllocZonelist()) == NULL)
      {
         DBFreeUcdmesh(um);
         return NULL;
      }
      *(um->zones) = tmpzones;

      CHECK_TYPE(type, DB_ZONELIST, zlname);

      um->zones->min_index = lo_offset;
      um->zones->max_index = um->zones->nzones - hi_offset - 1;

      /*----------------------------------------------------------*/
      /* If we have ghost zones, split any group of shapecnt so   */
      /* all the shapecnt refer to all real zones or all ghost    */
      /* zones.  This will make dealing with ghost zones easier   */
      /* for applications.                                        */
      /*----------------------------------------------------------*/
      if (lo_offset != 0 || hi_offset != 0)
      {
          db_SplitShapelist (um);
      }
   }

   if (elname && *elname) {
      DBedgelist tmpedges;
      memset(&tmpedges, 0, sizeof(DBedgelist));


      /*------------------------------------------------------------*/
      /*          Comp. Name        Comp. Address     Data Type     */
      /*------------------------------------------------------------*/
      INIT_OBJ(&tmp_obj);

      DEFINE_OBJ("ndims", &tmpedges.ndims, DB_INT);
      DEFINE_OBJ("nedges", &tmpedges.nedges, DB_INT);
      DEFINE_OBJ("origin", &tmpedges.origin, DB_INT);

      DEFALL_OBJ("edge_beg", &tmpedges.edge_beg, DB_INT);
      DEFALL_OBJ("edge_end", &tmpedges.edge_end, DB_INT);

      if (PJ_GetObject(dbfile->pdb, elname, &tmp_obj, NULL) < 0)
      {
         DBFreeUcdmesh(um);
         return NULL;
      }
      if ((um->edges = DBAllocEdgelist()) == NULL)
      {
         DBFreeUcdmesh(um);
         return NULL;
      }
      *(um->edges) = tmpedges;
   }

   if (phzlname && *phzlname && (SILO_Globals.dataReadMask & DBUMZonelist)) {
      um->phzones = db_pdb_GetPHZonelist(_dbfile, phzlname);
   }

   FREE (zlname);
   FREE (flname);
   FREE (elname);
   FREE (phzlname);

   return (um);

}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_GetUcdvar
 *
 *  Purpose
 *
 *      Read a ucd-var object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Modifications
 *
 *      Al Leibee, Tue Apr 19 08:56:11 PDT 1994
 *      Added dtime.
 *
 *      Robb Matzke, Tue Nov 15 11:05:44 EST 1994
 *      Device independence rewrite.
 *
 *      Sean Ahern, Tue Dec  5 16:38:55 PST 1995
 *      Added PDB logic to determine datatype
 *      if it's not in the SILO file.
 *
 *      Eric Brugger, Thu May 23 10:02:33 PDT 1996
 *      I corrected a bug where the datatype was being incorrectly set
 *      to DOUBLE, if force single were on and the datatype was
 *      specified in the silo file.
 *
 *      Sean Ahern, Fri May 24 17:20:00 PDT 1996
 *      I corrected a possible bug where, if force single were on,
 *      the datatype would only be set to DB_FLOAT if the datatype
 *      had previously been DB_DOUBLE.  This ignored things like
 *      DB_INT.
 *
 *      Eric Brugger, Fri Sep  6 08:12:40 PDT 1996
 *      I removed the reading of "meshid" since it is not used and
 *      is actually stored as a string in the silo files not an
 *      integer.  This caused a memory overwrite which only hurt
 *      things if the string was more than 48 characters.
 *
 *      Sean Ahern, Wed Jun 14 17:24:41 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *
 *      Brad Whitlock, Wed Jan 18 15:07:57 PST 2006
 *      Added optional ascii_labels.
 *
 *--------------------------------------------------------------------*/
CALLBACK DBucdvar *
db_pdb_GetUcdvar (DBfile *_dbfile, char *objname)
{
   char *type = NULL;
   DBucdvar      *uv = NULL;
   int            i;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   char           tmp[256];
   static char *me = "db_pdb_GetUcdvar";
   DBucdvar tmpuv;
   memset(&tmpuv, 0, sizeof(DBucdvar));


   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   DEFINE_OBJ("cycle", &tmpuv.cycle, DB_INT);
   DEFINE_OBJ("time", &tmpuv.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmpuv.dtime, DB_DOUBLE);
   DEFINE_OBJ("datatype", &tmpuv.datatype, DB_INT);
   DEFINE_OBJ("centering", &tmpuv.centering, DB_INT);
   DEFINE_OBJ("ndims", &tmpuv.ndims, DB_INT);
   DEFINE_OBJ("nels", &tmpuv.nels, DB_INT);
   DEFINE_OBJ("nvals", &tmpuv.nvals, DB_INT);
   DEFINE_OBJ("origin", &tmpuv.origin, DB_INT);
   DEFINE_OBJ("mixlen", &tmpuv.mixlen, DB_INT);
   DEFINE_OBJ("use_specmf", &tmpuv.use_specmf, DB_INT);
   DEFINE_OBJ("ascii_labels", &tmpuv.ascii_labels, DB_INT);

   DEFALL_OBJ("label", &tmpuv.label, DB_CHAR);
   DEFALL_OBJ("units", &tmpuv.units, DB_CHAR);
   DEFALL_OBJ("meshid",&tmpuv.meshname, DB_CHAR);
   DEFINE_OBJ("guihide", &tmpuv.guihide, DB_INT);

   if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
      return NULL;
   if ((uv = DBAllocUcdvar()) == NULL)
      return NULL;
   *uv = tmpuv;
   CHECK_TYPE(type, DB_UCDVAR, objname);

   /*
    *  Read the remainder of the object: loop over all values
    *  associated with this variable.
    */
   if ((uv->nvals > 0) && (SILO_Globals.dataReadMask & DBUVData)) {
      INIT_OBJ(&tmp_obj);

      uv->vals = ALLOC_N(float *, uv->nvals);

      if (uv->mixlen > 0) {
         uv->mixvals = ALLOC_N(float *, uv->nvals);
      }

      if (uv->datatype == 0) {
          strcpy(tmp, objname);
          strcat(tmp, "_data");
          if ((uv->datatype = db_pdb_GetVarDatatype(dbfile->pdb, tmp)) < 0) {
              /* Not found. Assume float. */
              uv->datatype = DB_FLOAT;
          }
      }

      if (PJ_InqForceSingle())
          uv->datatype = DB_FLOAT;

      for (i = 0; i < uv->nvals; i++) {
         DEFALL_OBJ(_valstr[i], &uv->vals[i], DB_FLOAT);

         if (uv->mixlen > 0) {
            DEFALL_OBJ(_mixvalstr[i], &uv->mixvals[i], DB_FLOAT);
         }
      }

      PJ_GetObject(dbfile->pdb, objname, &tmp_obj, NULL);
   }

   uv->id = 0;
   uv->name = STRDUP(objname);

   return (uv);
}

/*----------------------------------------------------------------------
 *  Routine                                                DBGetCsgmesh
 *
 *  Purpose
 *
 *      Read a CSG mesh structure from the given database.
 *
 *  Programmer
 *
 *      Mark C. Miller
 *      August 9, 2005
 *
 *--------------------------------------------------------------------*/
CALLBACK DBcsgmesh *
db_pdb_GetCsgmesh (DBfile *_dbfile, const char *meshname)
{
   char *type = NULL;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   DBcsgmesh     *csgm = NULL;
   char          *zlname = NULL, *tmpbndnames = NULL;
   PJcomplist     tmp_obj;
   int            lo_offset, hi_offset;
   static char *me = "db_pdb_GetCsgmesh";
   DBcsgmesh tmpcsgm;
   memset(&tmpcsgm, 0, sizeof(DBcsgmesh));

   /*------------------------------------------------------------*/
   /*          Comp. Name        Comp. Address     Data Type     */
   /*------------------------------------------------------------*/
   INIT_OBJ(&tmp_obj);

   DEFINE_OBJ("block_no", &tmpcsgm.block_no, DB_INT);
   DEFINE_OBJ("group_no", &tmpcsgm.group_no, DB_INT);
   DEFINE_OBJ("cycle", &tmpcsgm.cycle, DB_INT);
   DEFINE_OBJ("time", &tmpcsgm.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmpcsgm.dtime, DB_DOUBLE);
   DEFINE_OBJ("lcoeffs", &tmpcsgm.lcoeffs, DB_INT);
   DEFINE_OBJ("datatype", &tmpcsgm.datatype, DB_INT);
   DEFINE_OBJ("ndims", &tmpcsgm.ndims, DB_INT);
   DEFINE_OBJ("nbounds", &tmpcsgm.nbounds, DB_INT);
   DEFINE_OBJ("origin", &tmpcsgm.origin, DB_INT);
   DEFINE_OBJ("min_extents", tmpcsgm.min_extents, DB_DOUBLE);
   DEFINE_OBJ("max_extents", tmpcsgm.max_extents, DB_DOUBLE);
   DEFALL_OBJ("label0", &tmpcsgm.labels[0], DB_CHAR);
   DEFALL_OBJ("label1", &tmpcsgm.labels[1], DB_CHAR);
   DEFALL_OBJ("label2", &tmpcsgm.labels[2], DB_CHAR);
   DEFALL_OBJ("units0", &tmpcsgm.units[0], DB_CHAR);
   DEFALL_OBJ("units1", &tmpcsgm.units[1], DB_CHAR);
   DEFALL_OBJ("units2", &tmpcsgm.units[2], DB_CHAR);
   DEFALL_OBJ("csgzonelist", &zlname, DB_CHAR);
   DEFINE_OBJ("guihide", &tmpcsgm.guihide, DB_INT);

   if (SILO_Globals.dataReadMask & DBCSGMBoundaryInfo)
   {
       DEFALL_OBJ("typeflags", &tmpcsgm.typeflags, DB_INT);
       DEFALL_OBJ("bndids", &tmpcsgm.bndids, DB_INT);
   }

   /* Optional boundary names */
   if (SILO_Globals.dataReadMask & DBCSGMBoundaryNames)
       DEFALL_OBJ("bndnames", &tmpbndnames, DB_CHAR);

   if (PJ_GetObject(dbfile->pdb, (char*) meshname, &tmp_obj, &type) < 0)
      return NULL;
   CHECK_TYPE(type, DB_CSGMESH, meshname);

    /* now that we know the object's data type, we can correctly
       read the coeffs */
    if ((SILO_Globals.dataReadMask & DBCSGMBoundaryInfo) && (tmpcsgm.lcoeffs > 0))
    {
        INIT_OBJ(&tmp_obj);
        if (DB_DOUBLE == tmpcsgm.datatype && PJ_InqForceSingle()) {
           tmpcsgm.datatype = DB_FLOAT;
        }

        DEFALL_OBJ("coeffs", &tmpcsgm.coeffs, tmpcsgm.datatype);
        PJ_GetObject(dbfile->pdb, (char*) meshname, &tmp_obj, NULL);
    }

    if ((tmpbndnames != NULL) && (tmpcsgm.nbounds > 0))
    {
        tmpcsgm.bndnames = db_StringListToStringArray(tmpbndnames, tmpcsgm.nbounds);
        FREE(tmpbndnames);
    }

   tmpcsgm.name = STRDUP(meshname);

   if ((zlname && *zlname && (SILO_Globals.dataReadMask & DBCSGMZonelist)))
      tmpcsgm.zones = db_pdb_GetCSGZonelist(_dbfile, zlname);
  
   if ((csgm = DBAllocCsgmesh()) == NULL)
      return NULL;
   *csgm = tmpcsgm;

   FREE (zlname);

   return (csgm);

}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_GetCsgvar
 *
 *  Purpose
 *
 *      Read a csg-var object from a SILO file and return the
 *      SILO structure for this type.
 *
 *  Programmer
 *
 *      Mark C. Miller, August 10, 2005
 *
 *--------------------------------------------------------------------*/
CALLBACK DBcsgvar *
db_pdb_GetCsgvar (DBfile *_dbfile, const char *objname)
{
   char *type = NULL;
   DBcsgvar      *csgv = NULL;
   int            i;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   PJcomplist     tmp_obj;
   char           tmp[256];
   static char *me = "db_pdb_GetCsgvar";
   DBcsgvar tmpcsgv;
   memset(&tmpcsgv, 0, sizeof(DBcsgvar));

   INIT_OBJ(&tmp_obj);

   DEFINE_OBJ("cycle", &tmpcsgv.cycle, DB_INT);
   DEFINE_OBJ("time", &tmpcsgv.time, DB_FLOAT);
   DEFINE_OBJ("dtime", &tmpcsgv.dtime, DB_DOUBLE);
   DEFINE_OBJ("datatype", &tmpcsgv.datatype, DB_INT);
   DEFINE_OBJ("centering", &tmpcsgv.centering, DB_INT);
   DEFINE_OBJ("nels", &tmpcsgv.nels, DB_INT);
   DEFINE_OBJ("nvals", &tmpcsgv.nvals, DB_INT);
   DEFINE_OBJ("ascii_labels", &tmpcsgv.ascii_labels, DB_INT);
   DEFALL_OBJ("label", &tmpcsgv.label, DB_CHAR);
   DEFALL_OBJ("units", &tmpcsgv.units, DB_CHAR);
   DEFALL_OBJ("meshid",&tmpcsgv.meshname, DB_CHAR);
   DEFINE_OBJ("guihide", &tmpcsgv.guihide, DB_INT);

   if (PJ_GetObject(dbfile->pdb, (char*) objname, &tmp_obj, &type) < 0)
      return NULL;
   CHECK_TYPE(type, DB_CSGVAR, objname);

   /*
    *  Read the remainder of the object: loop over all values
    *  associated with this variable.
    */
   if ((tmpcsgv.nvals > 0) && (SILO_Globals.dataReadMask & DBCSGVData)) {
      INIT_OBJ(&tmp_obj);

      tmpcsgv.vals = ALLOC_N(void *, tmpcsgv.nvals);

      if (tmpcsgv.datatype == 0) {
          strcpy(tmp, objname);
          strcat(tmp, "_data");
          if ((tmpcsgv.datatype = db_pdb_GetVarDatatype(dbfile->pdb, tmp)) < 0) {
              /* Not found. Assume float. */
              tmpcsgv.datatype = DB_FLOAT;
          }
      }

      if ((tmpcsgv.datatype == DB_DOUBLE) && PJ_InqForceSingle())
          tmpcsgv.datatype = DB_FLOAT;

      for (i = 0; i < tmpcsgv.nvals; i++) {
         DEFALL_OBJ(_valstr[i], &tmpcsgv.vals[i], tmpcsgv.datatype);
      }

      PJ_GetObject(dbfile->pdb, (char*) objname, &tmp_obj, NULL);
   }

   tmpcsgv.name = STRDUP(objname);

   if ((csgv = DBAllocCsgvar()) == NULL)
      return NULL;
   *csgv = tmpcsgv;

   return (csgv);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetFacelist
 *
 * Purpose:     Reads a facelist object from a SILO file.
 *
 * Return:      Success:        A pointer to a new facelist structure.
 *
 *              Failure:        NULL
 *
 * Programmer:  Robb Matzke
 *              Friday, January 14, 2000
 *
 * Modifications:
 *
 *      Sean Ahern, Wed Jun 14 17:24:58 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBfacelist *
db_pdb_GetFacelist(DBfile *_dbfile, char *objname)
{
    char *type = NULL;
    DBfacelist          *fl=NULL;
    PJcomplist          tmp_obj;
    DBfile_pdb          *dbfile = (DBfile_pdb*)_dbfile;
    static char *me = "db_pdb_GetFacelist";
    DBfacelist tmpfl;
    memset(&tmpfl, 0, sizeof(DBfacelist));


    /*------------------------------------------------------------*/
    /*          Comp. Name        Comp. Address     Data Type     */
    /*------------------------------------------------------------*/
    INIT_OBJ(&tmp_obj);

    DEFINE_OBJ("ndims", &tmpfl.ndims, DB_INT);
    DEFINE_OBJ("nfaces", &tmpfl.nfaces, DB_INT);
    DEFINE_OBJ("origin", &tmpfl.origin, DB_INT);
    DEFINE_OBJ("lnodelist", &tmpfl.lnodelist, DB_INT);
    DEFINE_OBJ("nshapes", &tmpfl.nshapes, DB_INT);
    DEFINE_OBJ("ntypes", &tmpfl.ntypes, DB_INT);

    if (SILO_Globals.dataReadMask & DBFacelistInfo)
    {
        DEFALL_OBJ("nodelist", &tmpfl.nodelist, DB_INT);
        DEFALL_OBJ("shapecnt", &tmpfl.shapecnt, DB_INT);
        DEFALL_OBJ("shapesize", &tmpfl.shapesize, DB_INT);
        DEFALL_OBJ("typelist", &tmpfl.typelist, DB_INT);
        DEFALL_OBJ("types", &tmpfl.types, DB_INT);
        DEFALL_OBJ("nodeno", &tmpfl.nodeno, DB_INT);
        DEFALL_OBJ("zoneno", &tmpfl.zoneno, DB_INT);
    }

    if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
       return NULL;
    if ((fl = DBAllocFacelist()) == NULL)
       return NULL;
    *fl = tmpfl;
    CHECK_TYPE(type, DB_FACELIST, objname);

    return fl;
}


/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetZonelist
 *
 * Purpose:     Reads a zonelist object from a SILO file.
 *
 * Return:      Success:        A pointer to a new facelist structure.
 *
 *              Failure:        NULL
 *
 * Programmer:  Robb Matzke
 *              Friday, January 14, 2000
 *
 * Modifications:
 *
 *      Sean Ahern, Wed Jun 14 17:25:16 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBzonelist *
db_pdb_GetZonelist(DBfile *_dbfile, char *objname)
{
    char *type = NULL;
    DBzonelist          *zl = NULL;
    PJcomplist          tmp_obj;
    DBfile_pdb          *dbfile = (DBfile_pdb*)_dbfile;
    static char *me = "db_pdb_GetZonelist";
    DBzonelist tmpzl;
    memset(&tmpzl, 0, sizeof(DBzonelist));


    /*------------------------------------------------------------*/
    /*          Comp. Name        Comp. Address     Data Type     */
    /*------------------------------------------------------------*/
    INIT_OBJ(&tmp_obj);

    DEFINE_OBJ("ndims", &tmpzl.ndims, DB_INT);
    DEFINE_OBJ("nzones", &tmpzl.nzones, DB_INT);
    DEFINE_OBJ("origin", &tmpzl.origin, DB_INT);
    DEFINE_OBJ("lnodelist", &tmpzl.lnodelist, DB_INT);
    DEFINE_OBJ("nshapes", &tmpzl.nshapes, DB_INT);
    DEFINE_OBJ("min_index", &tmpzl.min_index, DB_INT);
    DEFINE_OBJ("max_index", &tmpzl.max_index, DB_INT);

    if (SILO_Globals.dataReadMask & DBZonelistInfo)
    {
        DEFALL_OBJ("shapecnt", &tmpzl.shapecnt, DB_INT);
        DEFALL_OBJ("shapesize", &tmpzl.shapesize, DB_INT);
        DEFALL_OBJ("shapetype", &tmpzl.shapetype, DB_INT);
        DEFALL_OBJ("nodelist", &tmpzl.nodelist, DB_INT);
        DEFALL_OBJ("zoneno", &tmpzl.zoneno, DB_INT);
    }

    if (SILO_Globals.dataReadMask & DBZonelistGlobZoneNo)
        DEFALL_OBJ("gzoneno", &tmpzl.gzoneno, DB_INT);

    if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
       return NULL;
    if ((zl = DBAllocZonelist()) == NULL)
       return NULL;
    *zl = tmpzl;
    CHECK_TYPE(type, DB_ZONELIST, objname);

    return zl;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetPHZonelist
 *
 * Purpose:     Reads a DBphzonelist object from a SILO file.
 *
 * Return:      Success:        A pointer to a new facelist structure.
 *
 *              Failure:        NULL
 *
 * Programmer:  Robb Matzke
 *              Friday, January 14, 2000
 *
 * Modifications:
 *
 *      Sean Ahern, Wed Jun 14 17:25:16 PDT 2000
 *      Added a check to make sure the object is the right type.
 *
 *      Sean Ahern, Thu Mar  1 12:28:07 PST 2001
 *      Added support for the dataReadMask stuff.
 *
 *      Mark C. Miller, Wed Feb  2 07:59:53 PST 2005
 *      Moved DBAlloc call to after PJ_GetObject. Added automatic
 *      var for PJ_GetObject to read into. Added check for return
 *      value of PJ_GetObject.
 *-------------------------------------------------------------------------*/
CALLBACK DBphzonelist *
db_pdb_GetPHZonelist(DBfile *_dbfile, char *objname)
{
    char *type = NULL;
    DBphzonelist          *phzl = NULL;
    PJcomplist          tmp_obj;
    DBfile_pdb          *dbfile = (DBfile_pdb*)_dbfile;
    static char *me = "db_pdb_GetPHZonelist";
    DBphzonelist tmpphzl;
    memset(&tmpphzl, 0, sizeof(DBphzonelist));


    /*------------------------------------------------------------*/
    /*          Comp. Name        Comp. Address     Data Type     */
    /*------------------------------------------------------------*/
    INIT_OBJ(&tmp_obj);

    DEFINE_OBJ("nfaces", &tmpphzl.nfaces, DB_INT);
    DEFINE_OBJ("lnodelist", &tmpphzl.lnodelist, DB_INT);
    DEFINE_OBJ("nzones", &tmpphzl.nzones, DB_INT);
    DEFINE_OBJ("lfacelist", &tmpphzl.lfacelist, DB_INT);
    DEFINE_OBJ("origin", &tmpphzl.origin, DB_INT);
    DEFINE_OBJ("lo_offset", &tmpphzl.lo_offset, DB_INT);
    DEFINE_OBJ("hi_offset", &tmpphzl.hi_offset, DB_INT);

    if (SILO_Globals.dataReadMask & DBZonelistInfo)
    {
        DEFALL_OBJ("nodecnt", &tmpphzl.nodecnt, DB_INT);
        DEFALL_OBJ("nodelist", &tmpphzl.nodelist, DB_INT);
        DEFALL_OBJ("extface", &tmpphzl.extface, DB_CHAR);
        DEFALL_OBJ("facecnt", &tmpphzl.facecnt, DB_INT);
        DEFALL_OBJ("facelist", &tmpphzl.facelist, DB_INT);
        DEFALL_OBJ("zoneno", &tmpphzl.zoneno, DB_INT);
    }

    if (SILO_Globals.dataReadMask & DBZonelistGlobZoneNo) 
        DEFALL_OBJ("gzoneno", &tmpphzl.gzoneno, DB_INT);

    if (PJ_GetObject(dbfile->pdb, objname, &tmp_obj, &type) < 0)
       return NULL;
    if ((phzl = DBAllocPHZonelist()) == NULL)
       return NULL;
    *phzl = tmpphzl;
    CHECK_TYPE(type, DB_PHZONELIST, objname);

    return phzl;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetCSGZonelist
 *
 * Purpose:     Reads a CSG zonelist object from a SILO file.
 *
 * Return:      Success:        A pointer to a new facelist structure.
 *
 *              Failure:        NULL
 *
 * Programmer:  Mark C. Miller 
 *              August 9, 2005 
 *
 *-------------------------------------------------------------------------*/
CALLBACK DBcsgzonelist *
db_pdb_GetCSGZonelist(DBfile *_dbfile, const char *objname)
{
    char *type = NULL;
    char *tmprnames = 0, *tmpznames = 0;
    DBcsgzonelist      *zl = NULL;
    PJcomplist          tmp_obj;
    DBfile_pdb         *dbfile = (DBfile_pdb*)_dbfile;
    static char *me = "db_pdb_GetCSGZonelist";
    DBcsgzonelist tmpzl;
    memset(&tmpzl, 0, sizeof(DBcsgzonelist));

    /*------------------------------------------------------------*/
    /*          Comp. Name        Comp. Address     Data Type     */
    /*------------------------------------------------------------*/
    INIT_OBJ(&tmp_obj);

    DEFINE_OBJ("nregs", &tmpzl.nregs, DB_INT);
    DEFINE_OBJ("origin", &tmpzl.origin, DB_INT);
    DEFINE_OBJ("lxform", &tmpzl.lxform, DB_INT);
    DEFINE_OBJ("datatype", &tmpzl.datatype, DB_INT);
    DEFINE_OBJ("nzones", &tmpzl.nzones, DB_INT);
    DEFINE_OBJ("min_index", &tmpzl.min_index, DB_INT);
    DEFINE_OBJ("max_index", &tmpzl.max_index, DB_INT);

    if (SILO_Globals.dataReadMask & DBZonelistInfo)
    {
        DEFALL_OBJ("typeflags", &tmpzl.typeflags, DB_INT);
        DEFALL_OBJ("leftids", &tmpzl.leftids, DB_INT);
        DEFALL_OBJ("rightids", &tmpzl.rightids, DB_INT);
        DEFALL_OBJ("zonelist", &tmpzl.zonelist, DB_INT);
    }
    if (SILO_Globals.dataReadMask & DBCSGZonelistRegNames)
        DEFALL_OBJ("regnames", &tmprnames, DB_CHAR);
    if (SILO_Globals.dataReadMask & DBCSGZonelistZoneNames)
        DEFALL_OBJ("zonenames", &tmpznames, DB_CHAR);

    if (PJ_GetObject(dbfile->pdb, (char*) objname, &tmp_obj, &type) < 0)
       return NULL;
    CHECK_TYPE(type, DB_CSGZONELIST, objname);

    /* now that we know the object's data type, we can correctly
       read the xforms */
    if ((SILO_Globals.dataReadMask & DBZonelistInfo) && (tmpzl.lxform > 0))
    {
        INIT_OBJ(&tmp_obj);
        if (DB_DOUBLE == tmpzl.datatype && PJ_InqForceSingle()) {
           tmpzl.datatype = DB_FLOAT;
        }

        DEFALL_OBJ("xform", &tmpzl.xform, tmpzl.datatype);
        PJ_GetObject(dbfile->pdb, (char*) objname, &tmp_obj, NULL);
    }

    if ((tmprnames != NULL) && (tmpzl.nregs > 0))
    {
        tmpzl.regnames = db_StringListToStringArray(tmprnames, tmpzl.nregs);
        FREE(tmprnames);
    }

    if ((tmpznames != NULL) && (tmpzl.nzones > 0))
    {
        tmpzl.zonenames = db_StringListToStringArray(tmpznames, tmpzl.nzones);
        FREE(tmpznames);
    }

    if ((zl = DBAllocCSGZonelist()) == NULL)
       return NULL;
    *zl = tmpzl;

    return zl;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetVar
 *
 * Purpose:     Allocates space for a variable and reads the variable
 *              from the database.
 *
 * Return:      Success:        Pointer to variable data
 *
 *              Failure:        NULL
 *
 * Programmer:  robb@cloud
 *              Tue Nov 15 11:12:23 EST 1994
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:13:19 PDT 1995
 *     Made "me" static.
 *-------------------------------------------------------------------------*/
CALLBACK void *
db_pdb_GetVar (DBfile *_dbfile, char *name)
{
   static char   *me = "db_pdb_GetVar";
   char          *data;
   int            n;

   /* Find out how long (in bytes) requested variable is. */
   if (0 == (n = DBGetVarByteLength(_dbfile, name))) {
      db_perror(name, E_NOTFOUND, me);
      return NULL;
   }

   /* Read it and return. */
   data = ALLOC_N(char, n);
   if (DBReadVar(_dbfile, name, data) < 0) {
      db_perror("DBReadVar", E_CALLFAIL, me);
      FREE(data);
      return (NULL);
   }

   return data;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetVarByteLength
 *
 * Purpose:     Returns the length of the given variable in bytes.
 *
 * Return:      Success:        length of variable in bytes
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Tue Nov 15 11:45:42 EST 1994
 *
 * Modifications:
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetVarByteLength (DBfile *_dbfile, char *varname)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   int            number, size;

   db_pdb_getvarinfo(dbfile->pdb, varname, NULL, &number, &size, 0);
   return number * size;
}


/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetVarDims
 *
 * Purpose:     Returns the size of each dimension.
 *
 * Return:      Success:        Number of dimensions not to exceed MAXDIMS.
 *                              The dimension sizes are written to DIMS.
 *
 *              Failure:        -1
 *
 * Programmer:  Robb Matzke
 *              robb@maya.nuance.mdn.com
 *              Mar  6 1997
 *
 * Modifications:
 *      Sean Ahern, Wed Apr 12 11:14:38 PDT 2000
 *      Removed the last two parameters to PJ_inquire_entry because they
 *      weren't being used.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetVarDims (DBfile *_dbfile, char *varname, int maxdims, int *dims)
{
   DBfile_pdb   *dbfile = (DBfile_pdb *) _dbfile;
   static char  *me = "db_pdb_GetVarDims";
   syment       *ep;
   dimdes       *dd;
   int          i;

   ep = PJ_inquire_entry (dbfile->pdb, varname);
   if (!ep) return db_perror ("PJ_inquire_entry", E_CALLFAIL, me);

   for (i=0, dd=ep->dimensions; i<maxdims && dd; i++, dd=dd->next) {
      dims[i] = dd->number;
   }
   return i;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetVarType
 *
 * Purpose:     Returns the data type ID (DB_INT, DB_FLOAT, etc) of the
 *              variable.
 *
 * Return:      Success:        type ID
 *
 *              Failure:        -1
 *
 * Programmer:  matzke@viper
 *              Thu Dec 22 09:00:39 PST 1994
 *
 * Modifications:
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetVarType (DBfile *_dbfile, char *varname)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   int            number, size;
   char           typename[256];

   db_pdb_getvarinfo(dbfile->pdb, varname, typename, &number, &size, 0);
   return db_GetDatatypeID(typename);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetVarLength
 *
 * Purpose:     Returns the number of elements in the requested variable.
 *
 * Return:      Success:        number of elements
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Tue Nov 15 12:06:36 EST 1994
 *
 * Modifications:
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetVarLength (DBfile *_dbfile, char *varname)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   int            number, size;

   db_pdb_getvarinfo(dbfile->pdb, varname, NULL, &number, &size, 0);
   return number;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_InqMeshname
 *
 * Purpose:     Returns the name of a mesh associated with a mesh-variable.
 *              Caller must allocate space for mesh name.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Tue Nov 15 12:10:59 EST 1994
 *
 * Modifications:
 *      Katherine Price, Fri Jun  2 08:58:54 PDT 1995
 *      Added error return code.
 *
 *      Sean Ahern, Mon Jun 24 13:30:53 PDT 1996
 *      Fixed a memory leak.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_InqMeshname (DBfile *_dbfile, char *vname, char *mname)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   void          *v;

   if ((v = PJ_GetComponent(dbfile->pdb, vname, "meshid"))) {
      if (mname)
         strcpy(mname, (char *)v);
      FREE(v);
      return 0;
   }
   FREE(v);
   return -1;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_InqMeshtype
 *
 * Purpose:     returns the mesh type.
 *
 * Return:      Success:        mesh type
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Tue Nov 15 12:16:12 EST 1994
 *
 * Modifications:
 *    Eric Brugger, Thu Feb  9 12:54:37 PST 1995
 *    I modified the routine to read elements of the group structure
 *    as "abc->type" instead of "abc.type".
 *
 *    Sean Ahern, Sun Oct  1 03:13:44 PDT 1995
 *    Made "me" static.
 *
 *    Eric Brugger, Fri Dec  4 12:45:22 PST 1998
 *    Added code to free ctype to eliminate a memory leak.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_InqMeshtype (DBfile *_dbfile, char *mname)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   char           tmp[256], *ctype;
   int            type;
   static char   *me = "db_pdb_InqMeshtype";

   sprintf(tmp, "%s->type", mname);
   if (!PJ_read(dbfile->pdb, tmp, &ctype)) {
      return db_perror("PJ_read", E_CALLFAIL, me);
   }

   type = DBGetObjtypeTag(ctype);
   SCFREE(ctype);

   return type;
}

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_InqVarExists
 *
 *  Purpose
 *      Check whether the variable exists and return non-zero if so,
 *      and 0 if not
 *
 *  Programmer
 *      Sean Ahern, Thu Jul 20 12:04:39 PDT 1995
 *
 *  Modifications
 *      Sean Ahern, Sun Oct  1 03:14:03 PDT 1995
 *      Made "me" static.
 *
 *      Sean Ahern, Wed Apr 12 11:14:38 PDT 2000
 *      Removed the last two parameters to PJ_inquire_entry because they
 *      weren't being used.
 *--------------------------------------------------------------------*/
CALLBACK int
db_pdb_InqVarExists (DBfile *_dbfile, char *varname)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   syment        *ep;

   ep = PJ_inquire_entry(dbfile->pdb, varname);
   if (!ep)
      return (0);
   else
      return (1);
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_ReadAtt
 *
 * Purpose:     Reads the specified attribute value into the provided
 *              space.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Mon Nov 21 21:00:56 EST 1994
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_ReadAtt(DBfile *_dbfile, char *vname, char *attname, void *results)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   char          *attval;

   attval = lite_PD_get_attribute(dbfile->pdb, vname, attname);
   memcpy(results, attval, lite_SC_arrlen(attval));
   SFREE(attval);

   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_ReadVar
 *
 * Purpose:     Reads a variable into the given space.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Mon Nov 21 21:05:07 EST 1994
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:18:45 PDT 1995
 *     Made "me" static.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_ReadVar (DBfile *_dbfile, char *vname, void *result)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_ReadVar";

   if (!PJ_read(dbfile->pdb, vname, result)) {
      return db_perror("PJ_read", E_CALLFAIL, me);
   }
   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_ReadVarSlice
 *
 * Purpose:     Reads a slice of a variable into the given space.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  brugger@sgibird
 *              Thu Feb 16 08:45:00 PST 1995
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:19:03 PDT 1995
 *     Made "me" static.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_ReadVarSlice (DBfile *_dbfile, char *vname, int *offset, int *length,
                     int *stride, int ndims, void *result)
{
   int            i;
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   long           ind[3 * MAXDIMS_VARWRITE];
   static char   *me = "db_pdb_ReadVarSlice";

   for (i = 0; i < ndims && i < MAXDIMS_VARWRITE; i++) {
      ind[3 * i] = offset[i];
      ind[3 * i + 1] = offset[i] + length[i] - 1;
      ind[3 * i + 2] = stride[i];
   }

   if (!PJ_read_alt(dbfile->pdb, vname, result, ind)) {
      return db_perror("PJ_read_alt", E_CALLFAIL, me);
   }
   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_SetDir
 *
 * Purpose:     Sets the current directory within the database.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Mon Nov 21 21:10:37 EST 1994
 *
 * Modifications:
 *    Eric Brugger, Fri Jan 27 08:27:46 PST 1995
 *    I changed the call DBGetToc to db_pdb_GetToc.
 *
 *    Robb Matzke, Fri Feb 24 10:15:54 EST 1995
 *    The directory ID (pub.dirid) is no longer set since PJ_pwd_id()
 *    has gone away.
 *
 *    Robb Matzke, Tue Mar 7 10:40:02 EST 1995
 *    I changed the call db_pdb_GetToc to DBNewToc.
 *
 *    Sean Ahern, Sun Oct  1 03:19:19 PDT 1995
 *    Made "me" static.
 *
 *    Sean Ahern, Mon Jul  1 14:06:08 PDT 1996
 *    Turned off the PJgroup cache when we change directories.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_SetDir (DBfile *_dbfile, char *path)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_SetDir";
   char           error_message[256];

   if (1 == lite_PD_cd(dbfile->pdb, path)) {
      dbfile->pub.dirid = 0;

      /* We've changed directories, so we can't cache PJgroups any more */
      PJ_NoCache();

      /* Must make new table-of-contents since dir has changed */
      db_FreeToc(_dbfile);
   }
   else {
      sprintf(error_message,"\"%s\" ***%s***",path,lite_PD_err);
      return db_perror(error_message, E_NOTDIR, me);
   }

   return 0;
}

/*-------------------------------------------------------------------------
 * Function:    db_pdb_Filters
 *
 * Purpose:     Output the name of this device driver to the specified
 *              stream.
 *
 * Return:      Success:        0
 *
 *              Failure:        never fails
 *
 * Programmer:  robb@cloud
 *              Tue Mar  7 11:11:59 EST 1995
 *
 * Modifications:
 *
 *    Hank Childs, Thu Jan  6 13:48:40 PST 2000
 *    Put in lint directive for unused arguments.
 *-------------------------------------------------------------------------*/
/* ARGSUSED */
CALLBACK int
db_pdb_Filters (DBfile *dbfile, FILE *stream)
{
   fprintf(stream, "PDB Device Driver\n");
   return 0;
}


/*-------------------------------------------------------------------------
 * Function:    db_pdb_GetComponentNames
 *
 * Purpose:     Returns the component names for the specified object.
 *              Each component name also has a variable name under which
 *              the component value is stored in the data file.  The
 *              COMP_NAMES and FILE_NAMES output arguments will point to
 *              an array of pointers to names.  Each name as well as the
 *              two arrays will be allocated with `malloc'.
 *
 * Return:      Success:        Number of components found for the
 *                              specified object.
 *
 *              Failure:        zero.
 *
 * Programmer:  Robb Matzke
 *              robb@callisto.nuance.mdn.com
 *              May 20, 1996
 *
 * Modifications:
 *
 *    Lisa J. Roberts, Tue Nov 23 09:39:49 PST 1999
 *    Changed strdup to safe_strdup.
 *-------------------------------------------------------------------------*/
CALLBACK int
db_pdb_GetComponentNames (DBfile *_dbfile, char *objname,
                          char ***comp_names, char ***file_names)
{
   PJgroup      *group = NULL ;
   DBfile_pdb   *dbfile = (DBfile_pdb *) _dbfile ;
   int          i ;
   syment       *ep ;

   if (comp_names) *comp_names = NULL ;
   if (file_names) *file_names = NULL ;

   /*
    * First make sure that the specified object is type "Group *", otherwise
    * we'll get a segmentation fault deep within the PDB library!
    */
   ep = lite_PD_inquire_entry (dbfile->pdb, objname, TRUE, NULL) ;
   if (!ep || strcmp(ep->type, "Group *")) return 0 ;

   /*
    * OK, now we can go ahead and get the group, but watch out
    * for the empty group.
    */
   if (!PJ_get_group (dbfile->pdb, objname, &group)) return 0 ;
   if (!group) return 0 ;
   if (group->ncomponents<=0) return 0 ;

   /*
    * Copy the group component names and pdb names into the
    * output arrays after allocating them.
    */
   if (comp_names)
       *comp_names = (char**)malloc(group->ncomponents * sizeof(char *)) ;
   if (file_names)
       *file_names = (char**)malloc(group->ncomponents * sizeof(char *)) ;
   for (i=0; i<group->ncomponents; i++) {
      if (comp_names) (*comp_names)[i] = safe_strdup (group->comp_names[i]) ;
      if (file_names) (*file_names)[i] = safe_strdup (group->pdb_names[i]) ;
   }

   return group->ncomponents ;
}

/*----------------------------------------------------------------------
 *  Routine                                         db_pdb_WriteObject
 *
 *  Purpose
 *
 *      Write an object into the given file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Returns
 *
 *      Returns OKAY on success, OOPS on failure.
 *
 * Modifications:
 *
 *      Robb Matzke, 7 Mar 1997
 *      Returns a failure status if PJ_put_group fails.
 *
 *      Robb Matzke, 7 Mar 1997
 *      The FREEMEM argument was replaced with a FLAGS argument that
 *      has the value 0, FREE_MEM, or OVER_WRITE depending on how
 *      this function got called (FREE_MEM still doesn't do anything).
 *
 *      Mark C. Miller, 10May06
 *      Passed value for SILO_Globals.allowOverwrites to PJ_put_group
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_WriteObject(DBfile   *_file,    /*File to write into */
                   DBobject *obj,      /*Object description to write out */
                   int      flags)     /*Sentinel: 1=free associated memory */
{
   PJgroup       *group;
   PDBfile       *file;
   static char   *me = "db_pdb_WriteObject";

   if (!obj || !_file)
      return (OOPS);
   file = ((DBfile_pdb *) _file)->pdb;

   group = PJ_make_group(obj->name, obj->type, obj->comp_names,
                         obj->pdb_names, obj->ncomponents);
   if (!PJ_put_group(file, group, (OVER_WRITE==flags?1:0) ||
                                  SILO_Globals.allowOverwrites)) {
      PJ_rel_group (group);
      return db_perror ("PJ_put_group", E_CALLFAIL, me);
   }
   PJ_rel_group(group);
   return (OKAY);
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                       db_pdb_WriteComponent
 *
 *  Purpose
 *
 *      Add a variable component to the given object structure, AND
 *      write out the associated data.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Returns
 *
 *      Returns OKAY on success, OOPS on failure.
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_WriteComponent (DBfile *_file, DBobject *obj, char *compname,
                       char *prefix, char *datatype, const void *var,
                       int nd, long count[])
{
   PDBfile       *file;
   char           tmp[256];

   file = ((DBfile_pdb *) _file)->pdb;
   db_mkname(file, prefix, compname, tmp);

   PJ_write_len(file, tmp, datatype, var, nd, count);
   DBAddVarComponent(obj, compname, tmp);
   return 0;
}
#endif /* PDB_WRITE */


/*-------------------------------------------------------------------------
 * Function:    db_pdb_Write
 *
 * Purpose:     Writes a single variable into a file.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Mon Nov 21 21:17:50 EST 1994
 *
 * Modifications:
 *
 *      Robb Matzke, Sun Dec 18 17:14:18 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString.
 *
 *      Sean Ahern, Sun Oct  1 03:19:43 PDT 1995
 *      Made "me" static.
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_Write (DBfile *_dbfile, char *vname, void *var,
              int *dims, int ndims, int datatype)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   char          *idata;
   int            i;
   long           ind[3 * MAXDIMS_VARWRITE];
   static char   *me = "db_pdb_Write";

   for (i = 0; i < ndims; i++) {
      ind[3 * i] = 0;
      ind[3 * i + 1] = dims[i] - 1;
      ind[3 * i + 2] = 1;
   }

   idata = db_GetDatatypeString(datatype);
   if (!PJ_write_alt(dbfile->pdb, vname, idata,
                     var, ndims, ind)) {
      return db_perror("PJ_write_alt", E_CALLFAIL, me);
   }
   FREE(idata);
   return 0;
}
#endif /* PDB_WRITE */


/*-------------------------------------------------------------------------
 * Function:    db_pdb_WriteSlice
 *
 * Purpose:     Similar to db_pdb_Write except only part of the data is
 *              written.  If VNAME doesn't exist, space is reserved for
 *              the entire variable based on DIMS; otherwise we check
 *              that DIMS has the same value as originally.  Then we
 *              write the specified slice to the file.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1
 *
 * Programmer:  Robb Matzke
 *              robb@callisto.nuance.com
 *              May  9, 1996
 *
 * Modifications:
 *      Sean Ahern, Tue Mar 31 11:15:03 PST 1998
 *      Freed dtype_s.
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_WriteSlice (DBfile *_dbfile, char *vname, void *values, int dtype,
                   int offset[], int length[], int stride[], int dims[],
                   int ndims)
{
   char         *dtype_s ;
   long         dim_extents[9] ;
   int          i ;
   DBfile_pdb   *dbfile = (DBfile_pdb*)_dbfile ;
   syment       *ep ;
   dimdes       *dimensions ;
   static char  *me = "db_pdb_WriteSlice" ;

   if (NULL==(dtype_s=db_GetDatatypeString (dtype))) {
      return db_perror ("db_GetDatatypeString", E_CALLFAIL, me) ;
   }

   if ((ep=lite_PD_inquire_entry (dbfile->pdb, vname, TRUE, NULL))) {
      /*
       * Variable already exists.  Make sure the supplied dimensions
       * are the same as what was originally used when space was reserved.
       */
      for (i=0, dimensions=ep->dimensions;
           i<ndims && dimensions;
           i++, dimensions=dimensions->next) {
         if (0!=dimensions->index_min) {
            return db_perror ("index_min!=0", E_BADARGS, me) ;
         }
         if (dimensions->number!=dims[i]) {
            return db_perror ("dims", E_BADARGS, me) ;
         }
      }
      if (i!=ndims) {
         return db_perror ("ndims", E_BADARGS, me) ;
      }
   } else {
      /*
       * Variable doesn't exist yet.  Reserve space for the variable
       * in the database and enter it's name in the symbol table.
       */
      for (i=0; i<3 && i<ndims; i++) {
         dim_extents[i*2+0] = 0 ;               /*minimum index*/
         dim_extents[i*2+1] = dims[i]-1 ;       /*maximum index*/
      }
      if (!lite_PD_defent_alt (dbfile->pdb, vname, dtype_s, ndims,
                               dim_extents)) {
         FREE(dtype_s);
         return db_perror ("PD_defent_alt", E_CALLFAIL, me) ;
      }
   }

   /*
    * Verify that offset and length are compatible with
    * the supplied dimensions.
    */
   for (i=0; i<ndims; i++) {
      if (offset[i]<0 || offset[i]>=dims[i]) {
         return db_perror ("offset", E_BADARGS, me) ;
      }
      if (length[i]<=0 || length[i]>dims[i]) {
         return db_perror ("length", E_BADARGS, me) ;
      }
      if (offset[i]+length[i]>dims[i]) {
         return db_perror ("offset+length", E_BADARGS, me) ;
      }
   }


   /*
    * Write the specified chunk of the values to the file.
    */
   for (i=0; i<3 && i<ndims; i++) {
      dim_extents[i*3+0] = offset[i] ;
      dim_extents[i*3+1] = offset[i] + length[i] - 1 ;
      dim_extents[i*3+2] = stride[i] ;
   }
   PJ_write_alt (dbfile->pdb, vname, dtype_s, values, ndims, dim_extents);
   FREE(dtype_s);

   return 0 ;
}
#endif /* PDB_WRITE */

/*-------------------------------------------------------------------------
 * Function:    db_pdb_MkDir
 *
 * Purpose:     Creates a new directory in the open database file.
 *
 * Return:      Success:        directory ID
 *
 *              Failure:        -1
 *
 * Programmer:  robb@cloud
 *              Tue Nov 15 15:13:44 EST 1994
 *
 * Modifications:
 *     Sean Ahern, Sun Oct  1 03:14:25 PDT 1995
 *     Made "me" static.
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_MkDir (DBfile *_dbfile, char *name)
{
   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   static char   *me = "db_pdb_MkDir";

   if (!lite_PD_mkdir(dbfile->pdb, name)) {
      return db_perror("PD_mkdir", E_CALLFAIL, me);
   }
   return 0;
}
#endif /* PDB_WRITE */


/*-------------------------------------------------------------------------
 * Function:    db_pdb_PutCompoundarray
 *
 * Purpose:     Put a compound array object into the PDB data file.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1, db_errno set
 *
 * Programmer:  matzke@viper
 *              Thu Nov  3 11:04:22 PST 1994
 *
 * Modifications:
 *
 *      Robb Matzke, Thu Nov 3 14:21:25 PST 1994
 *      Sets db_errno on failure.
 *
 *      Robb Matzke, Fri Dec 2 14:08:24 PST 1994
 *      Remove all references to SCORE memory management.
 *
 *      Robb Matzke, Sun Dec 18 17:04:29 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString and
 *      removed associated memory leak.
 *
 *      Sean Ahern, Sun Oct  1 03:09:48 PDT 1995
 *      Added "me" and made it static.
 *
 *      Hank Childs, Thu Jan  6 13:48:40 PST 2000
 *      Casted strlen to long to remove compiler warning.
 *      Put in lint directive for unused arguments.
 *
 *      Hank Childs, Wed Apr 11 08:05:24 PDT 2001
 *      Concatenate strings more intelligently [HYPer02535].
 *
 *      Hank Childs, Mon May 14 14:27:29 PDT 2001
 *      Fixed bug where there was an assumption that the string is
 *      NULL terminated.
 *
 *      Eric Brugger, Tue Apr 23 10:14:46 PDT 2002
 *      I modified the routine to add a ';' delimiter to the end of the
 *      name string so that GetCompoundarray would work properly since it
 *      makes that assumption.
 *
 *      Eric Brugger, Mon Sep 16 15:40:20 PDT 2002
 *      I corrected a bug where the routine would write out the string
 *      containing the component names one too large.  It still writes out
 *      the same number of characters, but now the array is one larger
 *      and the last character is set to a NULL character.  This seemed
 *      the safest thing to do.
 *
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
/* ARGSUSED */
CALLBACK int
db_pdb_PutCompoundarray (DBfile    *_dbfile,     /*pointer to open file  */
                         char      *array_name,  /*name of array object  */
                         char      *elemnames[], /*simple array names  */
                         int       *elemlengths, /*lengths of simple arrays */
                         int       nelems,       /*number of simple arrays */
                         void      *values,      /*vector of simple values */
                         int       nvalues,      /*num of values (redundant) */
                         int       datatype,     /*type of simple all values */
                         DBoptlist *optlist)     /*option list   */
{
   DBobject      *obj;
   char          *tmp, *cur;
   int            i, acc, len;
   long           count[3];

   /*
    * Build the list of simple array names in the format:
    *  `;name1;name2;...;nameN;'  The string must have a ';' at
    * the end for GetCompoundarray to work properly.
    */
   for (i = 0, acc = 1; i < nelems; i++)
      acc += strlen(elemnames[i]) + 1;
   acc++;
   tmp = ALLOC_N(char, acc);

   tmp[0] = '\0';
   cur = tmp;
   for (i = 0; i < nelems; i++) {
      strncpy(cur, ";", 1);
      cur += 1;
      len = strlen(elemnames[i]);
      strncpy(cur, elemnames[i], len);
      cur += len;
   }
   cur[0] = ';';
   cur++;
   cur[0] = '\0';

#if 0                           /*No global options available at this time */
   db_ProcessOptlist(DB_ARRAY, optlist);
#endif
   obj = DBMakeObject(array_name, DB_ARRAY, 25);

   /*
    * Write the compound array components to the database.
    */
   count[0] = (long) (cur - tmp) + 1;
   DBWriteComponent(_dbfile, obj, "elemnames", array_name,
                    "char", tmp, 1, count);
   FREE(tmp);

   count[0] = nelems;
   DBWriteComponent(_dbfile, obj, "elemlengths", array_name,
                    "integer", elemlengths, 1, count);

   DBAddIntComponent(obj, "nelems", nelems);

   count[0] = nvalues;
   tmp = db_GetDatatypeString(datatype);
   DBWriteComponent(_dbfile, obj, "values", array_name,
                    tmp, values, 1, count);
   FREE(tmp);

   DBAddIntComponent(obj, "nvalues", nvalues);
   DBAddIntComponent(obj, "datatype", datatype);

   DBWriteObject(_dbfile, obj, TRUE);
   DBFreeObject(obj);

   return OKAY;
}
#endif /* PDB_WRITE */


/*-------------------------------------------------------------------------
 * Function:    db_pdb_PutCurve
 *
 * Purpose:     Put a curve object into the PDB data file.
 *
 * Return:      Success:        0
 *
 *              Failure:        -1, db_errno set
 *
 * Programmer:  Robb Matzke
 *              robb@callisto.nuance.com
 *              May 15, 1996
 *
 * Modifications:
 *
 *      Thomas R. Treadway, Fri Jul  7 11:43:41 PDT 2006
 *      Added DBOPT_REFERENCE support.
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutCurve (DBfile *_dbfile, char *name, void *xvals, void *yvals,
                 int dtype, int npts, DBoptlist *opts)
{
   DBobject     *obj ;
   char         *dtype_s ;
   long         lnpts = npts ;
   static char  *me = "db_pdb_PutCurve" ;

   /*
    * Set global curve options to default values, then initialize
    * them with any values specified in the options list.
    */
   db_InitCurve (opts) ;
   obj = DBMakeObject (name, DB_CURVE, 18) ;

   /*
    * Write the X and Y arrays.  If the user specified a variable
    * name with the OPTS for the X or Y arrays then we assume that
    * the arrays have already been stored in the file.  This allows
    * us to share X values (or Y values) among several curves.
    * If a variable name is specified, then the corresponding X or
    * Y values array must be the null pointer!
    */
   dtype_s = db_GetDatatypeString (dtype) ;
   if (_cu._reference && (xvals || yvals)) {
      return db_perror ("vals argument can not be used with reference option",
                        E_BADARGS, me) ;
   }
   if (_cu._varname[0]) {
      if (xvals) {
         return db_perror ("xvals argument specified with xvarname option",
                           E_BADARGS, me) ;
      } else if (!_cu._varname[0]) {
         DBAddVarComponent (obj, "xvals", _cu._varname[0]) ;
      } 
   } else {
      if (!xvals && !_cu._reference) {
         return db_perror ("xvals", E_BADARGS, me) ;
      } else if (xvals && !_cu._reference) {
         DBWriteComponent (_dbfile, obj, "xvals", name, dtype_s,
                        xvals, 1, &lnpts);
      }
   }
   if (_cu._varname[1]) {
      if (yvals) {
         return db_perror ("yvals argument specified with yvarname option",
                           E_BADARGS, me) ;
      } else if (!_cu._varname[1]) {
         DBAddVarComponent (obj, "yvals", _cu._varname[1]) ;
      }
   } else {
      if (!yvals && !_cu._reference) {
         return db_perror ("yvals", E_BADARGS, me) ;
      } else if (yvals && !_cu._reference) {
         DBWriteComponent (_dbfile, obj, "yvals", name, dtype_s,
                        yvals, 1, &lnpts);
      }
   }
   FREE (dtype_s) ;

   /*
    * Now output the other values of the curve.
    */
   DBAddIntComponent (obj, "npts", npts) ;
   DBAddIntComponent (obj, "datatype", dtype) ;
   if (_cu._label)      DBAddStrComponent (obj, "label",    _cu._label) ;
   if (_cu._varname[0]) DBAddStrComponent (obj, "xvarname", _cu._varname[0]) ;
   if (_cu._labels[0])  DBAddStrComponent (obj, "xlabel",   _cu._labels[0]) ;
   if (_cu._units[0])   DBAddStrComponent (obj, "xunits",   _cu._units[0]) ;
   if (_cu._varname[1]) DBAddStrComponent (obj, "yvarname", _cu._varname[1]) ;
   if (_cu._labels[1])  DBAddStrComponent (obj, "ylabel",   _cu._labels[1]) ;
   if (_cu._units[1])   DBAddStrComponent (obj, "yunits",   _cu._units[1]) ;
   if (_cu._reference)  DBAddStrComponent (obj, "reference",_cu._reference) ;
   if (_cu._guihide)    DBAddIntComponent (obj, "guihide",  _cu._guihide);
   DBWriteObject (_dbfile, obj, TRUE) ;
   DBFreeObject(obj);

   return 0 ;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 * Routine                                           db_pdb_PutDefvars
 *
 * Purpose
 *
 *    Write a defvars object into the open SILO file.
 *
 * Programmer
 *
 *    Mark C. Miller
 *    August 8, 2005
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutDefvars (DBfile *dbfile, const char *name, int ndefs,
                     const char *names[], const int types[],
                     const char *defns[], DBoptlist *optlists[]) {

   int            i, len;
   long           count[1];
   DBobject      *obj;
   char          *tmp = NULL, *cur = NULL;
   int           *guihide = NULL;

   /*
    * Optlists are a little funky for this object because we were
    * concerned about possibly handling things like units, etc. So,
    * we really have an array of optlists that needs to get serialized.
    */
   if (optlists)
   {
       for (i = 0; i < ndefs; i++)
       {
           db_InitDefvars(optlists[i]);
           if (_dv._guihide)
           {
               if (guihide == NULL)
                   guihide = (int* ) calloc(ndefs, sizeof(int));
               guihide[i] = _dv._guihide;
           }
       }
   }

   /*-------------------------------------------------------------
    *  Build object description from literals and var-id's
    *-------------------------------------------------------------*/
   obj = DBMakeObject(name, DB_DEFVARS, 10);
   DBAddIntComponent(obj, "ndefs", ndefs);

   /*-------------------------------------------------------------
    *  Define and write variables types
    *-------------------------------------------------------------*/
   count[0] = ndefs;
   DBWriteComponent(dbfile, obj, "types", name, "integer",
                    (int*) types, 1, count);

   /*-------------------------------------------------------------
    *  Define and write variable names 
    *-------------------------------------------------------------*/
   db_StringArrayToStringList(names, ndefs, &tmp, &len);
   count[0] = len;  
   DBWriteComponent(dbfile, obj, "names", name, "char",
                    tmp, 1, count);
   FREE(tmp);
   tmp = NULL;

   /*-------------------------------------------------------------
    *  Define and write variable definitions 
    *-------------------------------------------------------------*/
   db_StringArrayToStringList(defns, ndefs, &tmp, &len);
   count[0] = len;  
   DBWriteComponent(dbfile, obj, "defns", name, "char",
                    tmp, 1, count);
   FREE(tmp);
   tmp = NULL;

   if (guihide != NULL) {

      count[0] = ndefs;
      DBWriteComponent(dbfile, obj, "guihide", name, "integer", guihide,
                       1, count);
      free(guihide);
   }

   /*-------------------------------------------------------------
    *  Write defvars object to SILO file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_PutFacelist
 *
 *  Purpose
 *
 *      Write a ucd facelist object into the open output file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modified
 *
 *      Robb Matzke, Wed Nov 16 11:39:19 EST 1994
 *      Added device independence.
 *
 *      Robb Matzke, Fri Dec 2 14:12:48 PST 1994
 *      Changed SCFREE(obj) to DBFreeObject(obj)
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutFacelist (DBfile *dbfile, char *name, int nfaces, int ndims,
                    int *nodelist, int lnodelist, int origin,
                    int *zoneno, int *shapesize, int *shapecnt, int nshapes,
                    int *types, int *typelist, int ntypes)
{
   long           count[5];
   DBobject      *obj;

   /*--------------------------------------------------
    *  Build up object description by defining literals
    *  and defining/writing arrays.
    *-------------------------------------------------*/
   obj = DBMakeObject(name, DB_FACELIST, 15);

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nfaces", nfaces);
   DBAddIntComponent(obj, "nshapes", nshapes);
   DBAddIntComponent(obj, "ntypes", ntypes);
   DBAddIntComponent(obj, "lnodelist", lnodelist);
   DBAddIntComponent(obj, "origin", origin);

   count[0] = lnodelist;

   DBWriteComponent(dbfile, obj, "nodelist", name, "integer",
                    nodelist, 1, count);

   if (ndims == 3) {
      count[0] = nshapes;

      DBWriteComponent(dbfile, obj, "shapecnt", name, "integer",
                       shapecnt, 1, count);

      DBWriteComponent(dbfile, obj, "shapesize", name, "integer",
                       shapesize, 1, count);
   }

   if (ntypes > 0 && typelist != NULL) {
      count[0] = ntypes;
      DBWriteComponent(dbfile, obj, "typelist", name, "integer",
                       typelist, 1, count);
   }

   if (ntypes > 0 && types != NULL) {
      count[0] = nfaces;
      DBWriteComponent(dbfile, obj, "types", name, "integer",
                       types, 1, count);
   }

   if (zoneno != NULL) {
      count[0] = nfaces;
      DBWriteComponent(dbfile, obj, "zoneno", name, "integer",
                       zoneno, 1, count);
   }

   /*-------------------------------------------------------------
    *  Write object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_PutMaterial
 *
 *  Purpose
 *
 *      Write a material object into the open SILO file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *      See the prologue for DBPutMaterial for a description of this
 *      data.
 *
 *  Modifications
 *
 *      Robb Matzke, Sun Dec 18 17:05:39 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString and removed
 *      associated memory leak.
 *
 *      Robb Matzke, Fri Dec 2 14:13:55 PST 1994
 *      Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *      Robb Matzke, Wed Nov 16 11:45:08 EST 1994
 *      Added device independence.
 *
 *      Al Leibee, Tue Jul 13 17:14:31 PDT 1993
 *      FREE of obj to SCFREE for consistant MemMan usage.
 *
 *      Sean Ahern, Wed Jan 17 17:02:31 PST 1996
 *      Added writing of the datatype parameter.
 *
 *      Sean Ahern, Tue Feb  5 10:19:53 PST 2002
 *      Added naming of Silo materials.
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMaterial (DBfile *dbfile, char *name, char *mname,
                    int nmat, int matnos[], int matlist[],
                    int dims[], int ndims,
                    int mix_next[], int mix_mat[], int mix_zone[],
                    float mix_vf[], int mixlen, int datatype,
                    DBoptlist *optlist)
{
   int            i, nels;
   long           count[3];
   DBobject      *obj;
   char          *datatype_str;

   /*-------------------------------------------------------------
    *  Process option list; build object description.
    *-------------------------------------------------------------*/
   db_ProcessOptlist(DB_MATERIAL, optlist);
   obj = DBMakeObject(name, DB_MATERIAL, 26);

   /*-------------------------------------------------------------
    * Define literals used by material data.
    *-------------------------------------------------------------*/

   DBAddStrComponent(obj, "meshid", mname);
   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nmat", nmat);
   DBAddIntComponent(obj, "mixlen", mixlen);
   DBAddIntComponent(obj, "origin", _ma._origin);
   DBAddIntComponent(obj, "major_order", _ma._majororder);
   DBAddIntComponent(obj, "datatype", datatype);
   if (_ma._guihide)
      DBAddIntComponent(obj, "guihide", _ma._guihide);

   /*-------------------------------------------------------------
    * Define variables, write them into object description.
    *-------------------------------------------------------------*/
   count[0] = ndims;
   DBWriteComponent(dbfile, obj, "dims", name, "integer", dims, 1, count);

   /* Do zonal material ID array */
   for (nels = 1, i = 0; i < ndims; i++)
      nels *= dims[i];

   count[0] = nels;
   DBWriteComponent(dbfile, obj, "matlist", name, "integer",
                    matlist, 1, count);

   /* Do material numbers list */
   count[0] = nmat;
   DBWriteComponent(dbfile, obj, "matnos", name, "integer",
                    matnos, 1, count);

   /* Now do mixed data arrays (mix_zone is optional) */
   if (mixlen > 0) {

      datatype_str = db_GetDatatypeString(datatype);
      count[0] = mixlen;
      DBWriteComponent(dbfile, obj, "mix_vf", name, datatype_str,
                       mix_vf, 1, count);
      FREE(datatype_str);

      DBWriteComponent(dbfile, obj, "mix_next", name, "integer",
                       mix_next, 1, count);
      DBWriteComponent(dbfile, obj, "mix_mat", name, "integer",
                       mix_mat, 1, count);

      if (mix_zone != NULL) {
         DBWriteComponent(dbfile, obj, "mix_zone", name, "integer",
                          mix_zone, 1, count);
      }
   }

    /* If we have material names, write them out */
   if (_ma._matnames != NULL)
   {
      int len; long llen; char *tmpstr = 0;
      db_StringArrayToStringList((const char**) _ma._matnames, nmat, &tmpstr, &len);
      llen = (long) len;
      DBWriteComponent(dbfile, obj, "matnames", name, "char", tmpstr, 1, &llen);
      FREE(tmpstr);
   }
   if (_ma._matcolors != NULL)
   {
      int len; long llen; char *tmpstr = 0;
      db_StringArrayToStringList((const char **) _ma._matcolors, nmat, &tmpstr, &len);
      llen = (long) len;
      DBWriteComponent(dbfile, obj, "matcolors", name, "char", tmpstr, 1, &llen);
      FREE(tmpstr);
   }

   /*-------------------------------------------------------------
    *  Write material object to output file. Request that underlying
    *  memory be freed (the 'TRUE' argument.)
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_PutMatspecies
 *
 *  Purpose
 *
 *      Write a material species object into the open SILO file.
 *
 *  Programmer
 *
 *      Al Leibee, DSAD-B
 *
 *  Notes
 *
 *      See the prologue for DBPutMatspecies for a description of this
 *      data.
 *
 *  Modifications
 *
 *      Robb Matzke, Fri Dec 2 14:14:27 PST 1994
 *      Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *      Robb Matzke, Sun Dec 18 17:06:28 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString and removed
 *      associated memory leak.
 *
 *      Jeremy Meredith, Wed Jul  7 12:15:31 PDT 1999
 *      I removed the origin value from the species object.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMatspecies (DBfile *dbfile, char *name, char *matname,
                      int nmat, int nmatspec[], int speclist[],
                      int dims[], int ndims, int nspecies_mf,
                      float species_mf[], int mix_speclist[],
                      int mixlen, int datatype, DBoptlist *optlist) {

   long           count[3];
   int            i, nels;
   DBobject      *obj;
   char          *datatype_str;

   /*-------------------------------------------------------------
    *  Process option list; build object description.
    *-------------------------------------------------------------*/
   db_ProcessOptlist(DB_MATSPECIES, optlist);
   obj = DBMakeObject(name, DB_MATSPECIES, 26);

   /*-------------------------------------------------------------
    * Define literals used by material species data.
    *-------------------------------------------------------------*/
   DBAddStrComponent(obj, "matname", matname);
   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nmat", nmat);
   DBAddIntComponent(obj, "nspecies_mf", nspecies_mf);
   DBAddIntComponent(obj, "mixlen", mixlen);
   DBAddIntComponent(obj, "datatype", datatype);
   DBAddIntComponent(obj, "major_order", _ms._majororder);
   if (_ms._guihide)
      DBAddIntComponent(obj, "guihide", _ms._guihide);

   /*-------------------------------------------------------------
    * Define variables, write them into object description.
    *-------------------------------------------------------------*/
   count[0] = ndims;
   DBWriteComponent(dbfile, obj, "dims", name, "integer", dims, 1, count);

   /* Do zonal material species ID array */
   for (nels = 1, i = 0; i < ndims; i++)
      nels *= dims[i];

   count[0] = nels;
   DBWriteComponent(dbfile, obj, "speclist", name, "integer",
                    speclist, 1, count);

   /* Do material species count per material list */
   count[0] = nmat;
   DBWriteComponent(dbfile, obj, "nmatspec", name, "integer",
                    nmatspec, 1, count);

   /* Do material species mass fractions */
   datatype_str = db_GetDatatypeString(datatype);
   count[0] = nspecies_mf;
   DBWriteComponent(dbfile, obj, "species_mf", name, datatype_str,
                    species_mf, 1, count);
   FREE(datatype_str);

   /* Now do mixed data arrays */
   if (mixlen > 0) {
      count[0] = mixlen;
      DBWriteComponent(dbfile, obj, "mix_speclist", name, "integer",
                       mix_speclist, 1, count);
   }

   /*-------------------------------------------------------------
    *  Write material object to output file. Request that underlying
    *  memory be freed (the 'TRUE' argument.)
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 * Routine                                           db_pdb_PutMultimesh
 *
 * Purpose
 *
 *    Write a multi-block mesh object into the open SILO file.
 *
 * Programmer
 *
 *    Jeffery W. Long, NSSD-B
 *
 * Notes
 *
 * Modified
 *    Robb Matzke, Fri Dec 2 14:15:31 PST 1994
 *    Changed  SCFREE(obj) to DBFreeObject(obj)
 *
 *    Eric Brugger, Fri Jan 12 17:42:55 PST 1996
 *    I added cycle, time and dtime as options.
 *
 *    Sean Ahern, Thu Aug 15 11:16:18 PDT 1996
 *    Allowed the mesh names to be any length, rather than hardcoded.
 *
 *    Eric Brugger, Fri Oct 17 09:11:58 PDT 1997
 *    I corrected the outputing of the cyle, time and dtime options to
 *    use the values from the global _mm instead of _pm.  A cut
 *    and paste error.
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *    Casted a strlen to long to remove compiler warning.
 *
 *    Hank Childs, Wed Apr 11 08:05:24 PDT 2001
 *    Concatenate strings more intelligently [HYPer02535].
 *
 *    Hank Childs, Mon May 14 14:27:29 PDT 2001
 *    Fixed bug where there was an assumption that the string is
 *    NULL terminated.
 *
 *    Thomas R. Treadway, Thu Jul 20 13:34:57 PDT 2006
 *    Added lgroupings, groupings, and groupnames options.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMultimesh (DBfile *dbfile, char *name, int nmesh,
                     char *meshnames[], int meshtypes[],
                     DBoptlist *optlist) {

   int            i, len;
   long           count[3];
   DBobject      *obj;
   char          *tmp = NULL, *cur = NULL;
   char          *gtmp = NULL;

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   db_InitMulti(dbfile, optlist);

   /*-------------------------------------------------------------
    *  Build object description from literals and var-id's
    *-------------------------------------------------------------*/
   obj = DBMakeObject(name, DB_MULTIMESH, 18);
   DBAddIntComponent(obj, "nblocks", nmesh);
   DBAddIntComponent(obj, "ngroups", _mm._ngroups);
   DBAddIntComponent(obj, "blockorigin", _mm._blockorigin);
   DBAddIntComponent(obj, "grouporigin", _mm._grouporigin);
   if (_mm._guihide)
      DBAddIntComponent(obj, "guihide", _mm._guihide);

   /*-------------------------------------------------------------
    *  Define and write variables before adding them to object.
    *-------------------------------------------------------------*/
   count[0] = nmesh;

   DBWriteComponent(dbfile, obj, "meshtypes", name, "integer",
                    meshtypes, 1, count);

    /* Compute size needed for string of concatenated block names.
     *
     * Note that we start with 2 so that we have one `;' and the NULL
     * terminator.  Also, the +1 in the "len +=" line is for the `;'
     * character.
     */
    len = 2;
    for(i=0; i<nmesh; i++)
    {
        len += strlen(meshnames[i]) + 1;
    }
    tmp = ALLOC_N(char,len);

   /* Build 1-D character string from 2-D mesh-name array */
   tmp[0] = ';';
   tmp[1] = '\0';

   cur = tmp+1;
   for (i = 0; i < nmesh; i++) {
      int len2;
      len2 = strlen(meshnames[i]);
      strncpy(cur, meshnames[i], len2);
      cur += len2;
      strncpy(cur, ";", 1);
      cur += 1;
   }

   count[0] = (long) (cur - tmp);
   DBWriteComponent(dbfile, obj, "meshnames", name, "char",
                    tmp, 1, count);

   /*-------------------------------------------------------------
    *  Define and write the time and cycle.
    *-------------------------------------------------------------*/
   DBAddIntComponent(obj, "cycle", _mm._cycle);

   if (_mm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _mm._nm_time);
   if (_mm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _mm._nm_dtime);

   /*-------------------------------------------------------------
    *  Add the DBOPT_EXTENTS_SIZE and DBOPT_EXTENTS options if present.
    *-------------------------------------------------------------*/
   if (_mm._extents != NULL && _mm._extentssize > 0) {
      DBAddIntComponent(obj, "extentssize", _mm._extentssize);

      count[0] = _mm._extentssize * nmesh;
      DBWriteComponent(dbfile, obj, "extents", name, "double", _mm._extents,
                       1, count);
   }

   /*-------------------------------------------------------------
    *  Add the DBOPT_ZONECOUNTS option if present.
    *-------------------------------------------------------------*/
   if (_mm._zonecounts != NULL) {

      count[0] = nmesh;
      DBWriteComponent(dbfile, obj, "zonecounts", name, "integer", 
                       _mm._zonecounts, 1, count);
   }

   /*-------------------------------------------------------------
    *  Add the DBOPT_HAS_EXTERNAL_ZONES option if present.
    *-------------------------------------------------------------*/
   if (_mm._has_external_zones != NULL) {

      count[0] = nmesh;
      DBWriteComponent(dbfile, obj, "has_external_zones", name, "integer", 
                       _mm._has_external_zones, 1, count);
   }
   /*-------------------------------------------------------------
    *  Add the DBOPT_GROUP* options if present.
    *-------------------------------------------------------------*/
   if (_mm._lgroupings > 0)
      DBAddIntComponent(obj, "lgroupings", _mm._lgroupings);
   if ((_mm._lgroupings  > 0) && (_mm._groupnames != NULL)) {
      db_StringArrayToStringList((const char **) _mm._groupnames, 
                    _mm._lgroupings, &gtmp, &len);

      count[0] = len;
      DBWriteComponent(dbfile, obj, "groupnames", name, "char",
                    gtmp, 1, count);
      FREE(gtmp);
   }
   if ((_mm._lgroupings  > 0) && (_mm._groupings != NULL)) {
      count[0] = _mm._lgroupings;
      DBWriteComponent(dbfile, obj, "groupings", name, "integer",
                    _mm._groupings, 1, count);
   }

   /*-------------------------------------------------------------
    *  Write multi-mesh object to SILO file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   FREE(tmp);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 * Routine                                        db_pdb_PutMultimeshadj
 *
 * Purpose
 *
 *    Write some or all of a multi-block mesh adjacency object into the
 *    open SILO file. This routine is designed to permit multiple
 *    writes to the same object with different pieces being written
 *    each time.
 *
 * Programmer
 *
 *    Mark C. Miller, August 23, 2005 
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMultimeshadj (DBfile *_dbfile, const char *name, int nmesh,
                  const int *meshtypes, const int *nneighbors,
                  const int *neighbors, const int *back,
                  const int *lnodelists, const int *nodelists[],
                  const int *lzonelists, const int *zonelists[],
                  DBoptlist *optlist) {


   long         count[2];
   int          i, len, noff, zoff, lneighbors;
   DBfile_pdb   *dbfile = (DBfile_pdb*)_dbfile;
   syment       *ep;
   dimdes       *dimensions;
   static char  *me = "db_pdb_PutMultimeshadj";
   char tmpn[256];

   if ((ep=lite_PD_inquire_entry (dbfile->pdb, (char*)name, TRUE, NULL))) {
      /*
       * Object already exists. Do whatever sanity checking we can do without
       * reading back problem-sized data. Basically, this means checking
       * existence and size of various components.
       */

      /* meshtypes should *always* exist and be of size nmesh */
      db_mkname(dbfile->pdb, (char*)name, "meshtypes", tmpn);
      if ((ep=lite_PD_inquire_entry(dbfile->pdb, tmpn, TRUE, NULL)))
      {
         len = 0;
         for (dimensions=ep->dimensions; dimensions; dimensions=dimensions->next)
            len += dimensions->number;

         if (len != nmesh)
            return db_perror("inconsistent meshtypes", E_BADARGS, me);
      }
      else
      {
         return db_perror("not a DBmultimeshadj object", E_BADARGS, me);
      }

      /* nneirhbors should *always* exist and be of size nmesh */
      db_mkname(dbfile->pdb, (char*)name, "nneighbors", tmpn);
      if ((ep=lite_PD_inquire_entry(dbfile->pdb, tmpn, TRUE, NULL)))
      {
         len = 0;
         for (dimensions=ep->dimensions; dimensions; dimensions=dimensions->next)
            len += dimensions->number;

         if (len != nmesh)
            return db_perror("inconsistent nneighbors", E_BADARGS, me);
      }
      else
      {
         return db_perror("not a DBmultimeshadj object", E_BADARGS, me);
      }

      /* compute expected size of neighbors array */
      lneighbors = 0;
      for (i = 0; i < nmesh; i++)
          lneighbors += nneighbors[i];

      /* neighbors should always exist and be of size lneighbors */
      db_mkname(dbfile->pdb, (char*)name, "neighbors", tmpn);
      if ((ep=lite_PD_inquire_entry(dbfile->pdb, tmpn, TRUE, NULL)))
      {
         len = 0;
         for (dimensions=ep->dimensions; dimensions; dimensions=dimensions->next)
            len += dimensions->number;

         if (len != lneighbors)
            return db_perror("inconsistent neighbors", E_BADARGS, me);
      }
      else
      {
         return db_perror("not a DBmultimeshadj object", E_BADARGS, me);
      }

      /* if lnodelists exists, it should be of size lneighbors AND it
         should be non-NULL in the argument list. Otherwise, it should be NULL */
      db_mkname(dbfile->pdb, (char*)name, "lnodelists", tmpn);
      if ((ep=lite_PD_inquire_entry(dbfile->pdb, tmpn, TRUE, NULL)))
      {
         if (lnodelists == 0)
            return db_perror("inconsistent lnodelists", E_BADARGS, me);
         len = 0;
         for (dimensions=ep->dimensions; dimensions; dimensions=dimensions->next)
            len += dimensions->number;
         if (len != lneighbors)
            return db_perror("inconsistent lnodelists", E_BADARGS, me);
      }
      else
      {
         if (lnodelists != 0)
            return db_perror("inconsistent lnodelists", E_BADARGS, me);
      }

      /* if lzonelists exists, it should be of size lneighbors AND it
         should be non-NULL in the argument list. Otherwise, it should be NULL */
      db_mkname(dbfile->pdb, (char*)name, "lzonelists", tmpn);
      if ((ep=lite_PD_inquire_entry(dbfile->pdb, tmpn, TRUE, NULL)))
      {
         if (lzonelists == 0)
            return db_perror("inconsistent lzonelists", E_BADARGS, me);
         len = 0;
         for (dimensions=ep->dimensions; dimensions; dimensions=dimensions->next)
            len += dimensions->number;
         if (len != lneighbors)
            return db_perror("inconsistent lzonelists", E_BADARGS, me);
      }
      else
      {
         if (lzonelists != 0)
            return db_perror("inconsistent lzonelists", E_BADARGS, me);
      }

   } else {
      /*
       * Object doesn't exist yet.  Write all the object's invariant
       * components and reserve space for the nodelists and/or
       * zonelists and enter names in the symbol table.
       */
      DBobject *obj;

      /* compute length of neighbors, back, lnodelists, nodelists,
         lzonelists, zonelists arrays */
      lneighbors = 0;
      for (i = 0; i < nmesh; i++)
          lneighbors += nneighbors[i];

      db_InitMulti(_dbfile, optlist);
      obj = DBMakeObject(name, DB_MULTIMESHADJ, 13);

      DBAddIntComponent(obj, "nblocks", nmesh);
      DBAddIntComponent(obj, "blockorigin", _mm._blockorigin);
      DBAddIntComponent(obj, "lneighbors", lneighbors);

      count[0] = nmesh;
      DBWriteComponent(_dbfile, obj, "meshtypes", name, "integer",
                       meshtypes, 1, count);
      DBWriteComponent(_dbfile, obj, "nneighbors", name, "integer",
                       nneighbors, 1, count);

      count[0] = lneighbors;
      DBWriteComponent(_dbfile, obj, "neighbors", name, "integer",
                       neighbors, 1, count);
      if (back) {
          DBWriteComponent(_dbfile, obj, "back", name, "integer",
                           back, 1, count);
      }
      if (lnodelists) {
          DBWriteComponent(_dbfile, obj, "lnodelists", name, "integer",
                           lnodelists, 1, count);
      }
      if (lzonelists) {
          DBWriteComponent(_dbfile, obj, "lzonelists", name, "integer",
                           lzonelists, 1, count);
      }

      /* All object components up to here are invariant and *should*
         be identical in repeated calls. Now, handle the parts of the
         object that can vary from call to call. Reserve space for
         the entire nodelists and/or zonelists arrays */

      if (nodelists) {

          /* compute total length of nodelists array */
          len = 0;
          for (i = 0; i < lneighbors; i++)
              len += lnodelists[i];
          DBAddIntComponent(obj, "totlnodelists", len);

          /* reserve space for the nodelists array in the file */
          count[0] = 0;
          count[1] = len - 1;
          db_mkname(dbfile->pdb, (char*)name, "nodelists", tmpn);
          if (!lite_PD_defent_alt (dbfile->pdb, tmpn, "integer", 1, count)) {
             return db_perror ("PD_defent_alt", E_CALLFAIL, me) ;
          }

          /* add the nodelists array to this object */
          DBAddVarComponent(obj, "nodelists", tmpn);
      }


      if (zonelists) {

          /* compute total length of nodelists array */
          len = 0;
          for (i = 0; i < lneighbors; i++)
              len += lzonelists[i];
          DBAddIntComponent(obj, "totlzonelists", len);

          /* reserve space for the nodelists array in the file */
          count[0] = 0;
          count[1] = len - 1;
          db_mkname(dbfile->pdb, (char*)name, "zonelists", tmpn);
          if (!lite_PD_defent_alt (dbfile->pdb, tmpn, "integer", 1, count)) {
             return db_perror ("PD_defent_alt", E_CALLFAIL, me) ;
          }

          /* add the nodelists array to this object */
          DBAddVarComponent(obj, "zonelists", tmpn);
      }

      /* Ok, finally, create the object in the file */
      DBWriteObject(_dbfile, obj, TRUE);
      DBFreeObject(obj);
   }

   /* Ok, now write contents of nodelists and/or zonelists */
   noff = 0;
   zoff = 0;
   for (i = 0; i < lneighbors; i++)
   {
      long dim_extents[3];

      if (nodelists)
      {
         if (nodelists[i])
         {
            dim_extents[0] = noff;
            dim_extents[1] = noff + lnodelists[i] - 1;
            dim_extents[2] = 1;
            db_mkname(dbfile->pdb, (char*)name, "nodelists", tmpn);
            PJ_write_alt (dbfile->pdb, tmpn, "integer", (int*) nodelists[i], 1, dim_extents);
         }
         noff += lnodelists[i];
      }

      if (zonelists)
      {
         if (zonelists[i])
         {
            dim_extents[0] = zoff;
            dim_extents[1] = zoff + lzonelists[i] - 1;
            dim_extents[2] = 1;
            db_mkname(dbfile->pdb, (char*)name, "zonelists", tmpn);
            PJ_write_alt (dbfile->pdb, tmpn, "integer", (int*) zonelists[i], 1, dim_extents);
         }
         zoff += lzonelists[i];
      }
   }

   return 0;
}
#endif


/*----------------------------------------------------------------------
 * Routine                                            db_pdb_PutMultivar
 *
 * Purpose
 *
 *    Write a multi-block variable object into the open SILO file.
 *
 * Programmer
 *
 *    Jeffery W. Long, NSSD-B
 *
 * Notes
 *
 * Modified
 *    Robb Matzke, Fri Dec 2 14:16:09 PST 1994
 *    Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *    Eric Brugger, Fri Jan 12 17:42:55 PST 1996
 *    I added cycle, time and dtime as options.
 *
 *    Sean Ahern, Thu Aug 15 11:16:18 PDT 1996
 *    Allowed the mesh names to be any length, rather than hardcoded.
 *
 *    Eric Brugger, Fri Oct 17 09:11:58 PDT 1997
 *    I corrected the outputing of the cyle, time and dtime options to
 *    use the values from the global _mm instead of _pm.  A cut
 *    and paste error.
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *    Casted a strlen to long to remove a compiler warning.
 *
 *    Hank Childs, Wed Apr 11 08:05:24 PDT 2001
 *    Concatenate strings more intelligently [HYPer02535].
 *
 *    Hank Childs, Mon May 14 14:27:29 PDT 2001
 *    Fixed bug where there was an assumption that the string is
 *    NULL terminated.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMultivar (DBfile *dbfile, char *name, int nvars,
                    char *varnames[], int vartypes[], DBoptlist *optlist) {

   int            i, len;
   long           count[3];
   char          *tmp = NULL, *cur = NULL;
   DBobject      *obj;

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   db_InitMulti(dbfile, optlist);

   /*-------------------------------------------------------------
    *  Build object description from literals and var-id's
    *-------------------------------------------------------------*/
   obj = DBMakeObject(name, DB_MULTIVAR, 13);
   DBAddIntComponent(obj, "nvars", nvars);
   DBAddIntComponent(obj, "ngroups", _mm._ngroups);
   DBAddIntComponent(obj, "blockorigin", _mm._blockorigin);
   DBAddIntComponent(obj, "grouporigin", _mm._grouporigin);
   if (_mm._guihide)
      DBAddIntComponent(obj, "guihide", _mm._guihide);

   /*-------------------------------------------------------------
    *  Define and write variables before adding them to object.
    *-------------------------------------------------------------*/
   count[0] = nvars;
   DBWriteComponent(dbfile, obj, "vartypes", name, "integer",
                    vartypes, 1, count);

    /* Compute size needed for string of concatenated block names.
     *
     * Note that we start with 2 so that we have one `;' and the NULL
     * terminator.  Also, the +1 in the "len +=" line is for the `;'
     * character.
     */
    len = 2;
    for(i=0; i<nvars; i++)
    {
        len += strlen(varnames[i]) + 1;
    }
    tmp = ALLOC_N(char,len);

   /* Build 1-D character string from 2-D mesh-name array */
   tmp[0] = ';';
   tmp[1] = '\0';

   cur = tmp+1;
   for (i = 0; i < nvars; i++) {
      int len2;
      len2 = strlen(varnames[i]);
      strncpy(cur, varnames[i], len2);
      cur += len2;
      strncpy(cur, ";", 1);
      cur += 1;
   }

   count[0] = (long) (cur - tmp);
   DBWriteComponent(dbfile, obj, "varnames", name, "char",
                    tmp, 1, count);

   /*-------------------------------------------------------------
    *  Define and write the time and cycle.
    *-------------------------------------------------------------*/
   DBAddIntComponent(obj, "cycle", _mm._cycle);

   if (_mm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _mm._nm_time);
   if (_mm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _mm._nm_dtime);

   /*-------------------------------------------------------------
    *  Add the DBOPT_EXTENTS_SIZE and DBOPT_EXTENTS options if present.
    *-------------------------------------------------------------*/
   if (_mm._extents != NULL && _mm._extentssize > 0) {
      DBAddIntComponent(obj, "extentssize", _mm._extentssize);

      count[0] = _mm._extentssize * nvars;
      DBWriteComponent(dbfile, obj, "extents", name, "double", _mm._extents,
                       1, count);
   }

   /*-------------------------------------------------------------
    *  Write multi-var object to SILO file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   FREE(tmp);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 * Routine                                            db_pdb_PutMultimat
 *
 * Purpose
 *
 *    Write a multi-material object into the open SILO file.
 *
 * Programmer
 *
 *    robb@cloud
 *    Tue Feb 21 12:42:07 EST 1995
 *
 * Notes
 *
 * Modified
 *    Eric Brugger, Fri Jan 12 17:42:55 PST 1996
 *    I added cycle, time and dtime as options.
 *
 *    Sean Ahern, Thu Aug 15 11:16:18 PDT 1996
 *    Allowed the mesh names to be any length, rather than hardcoded.
 *
 *    Eric Brugger, Fri Oct 17 09:11:58 PDT 1997
 *    I corrected the outputing of the cyle, time and dtime options to
 *    use the values from the global _mm instead of _pm.  A cut
 *    and paste error.  I added code to write out the DPOPT_NMATNOS
 *    and DPOPT_MATNOS options if provided.
 *
 *    Jeremy Meredith, Sept 23 1998
 *    Corrected a bug where 'browser' wouldn't read "matnos": changed
 *    type used when writing "matnos" from "int" to "integer".
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *    Cast a strlen to long to avoid a compiler warning.
 *
 *    Hank Childs, Wed Apr 11 08:05:24 PDT 2001
 *    Concatenate strings more intelligently [HYPer02535].
 *
 *    Hank Childs, Mon May 14 14:27:29 PDT 2001
 *    Fixed bug where there was an assumption that the string is
 *    NULL terminated.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMultimat (DBfile *dbfile, char *name, int nmats,
                    char *matnames[], DBoptlist *optlist) {

   int            i, len;
   long           count[3];
   char          *tmp = NULL, *cur = NULL;
   DBobject      *obj;

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   db_InitMulti(dbfile, optlist);

   /*-------------------------------------------------------------
    *  Build object description from literals and var-id's
    *-------------------------------------------------------------*/
   obj = DBMakeObject(name, DB_MULTIMAT, 14);
   DBAddIntComponent(obj, "nmats", nmats);
   DBAddIntComponent(obj, "ngroups", _mm._ngroups);
   DBAddIntComponent(obj, "blockorigin", _mm._blockorigin);
   DBAddIntComponent(obj, "grouporigin", _mm._grouporigin);
   if (_mm._guihide)
      DBAddIntComponent(obj, "guihide", _mm._guihide);

   /*-------------------------------------------------------------
    *  Define and write materials before adding them to object.
    *-------------------------------------------------------------*/

    /* Compute size needed for string of concatenated block names.
     *
     * Note that we start with 2 so that we have one `;' and the NULL
     * terminator.  Also, the +1 in the "len +=" line is for the `;'
     * character.
     */
    len = 2;
    for(i=0; i<nmats; i++)
    {
        len += strlen(matnames[i]) + 1;
    }
    tmp = ALLOC_N(char,len);

   /* Build 1-D character string from 2-D mesh-name array */
   tmp[0] = ';';
   tmp[1] = '\0';

   cur = tmp+1;
   for (i = 0; i < nmats; i++) {
      int len2;
      len2 = strlen(matnames[i]);
      strncpy(cur, matnames[i], len2);
      cur += len2;
      strncpy(cur, ";", 1);
      cur += 1;
   }

   count[0] = (long) (cur - tmp);
   DBWriteComponent(dbfile, obj, "matnames", name, "char",
                    tmp, 1, count);

   /*-------------------------------------------------------------
    *  Define and write the time and cycle.
    *-------------------------------------------------------------*/
   DBAddIntComponent(obj, "cycle", _mm._cycle);

   if (_mm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _mm._nm_time);
   if (_mm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _mm._nm_dtime);

   /*-------------------------------------------------------------
    *  Add the DBOPT_MATNOS and DBOPT_NMATNOS options if present.
    *-------------------------------------------------------------*/
   if (_mm._matnos != NULL && _mm._nmatnos > 0) {
      DBAddIntComponent(obj, "nmatnos", _mm._nmatnos);

      count[0] = _mm._nmatnos;
      DBWriteComponent(dbfile, obj, "matnos", name, "integer", _mm._matnos,
                       1, count);
   }

   /*-------------------------------------------------------------
    *  Add the DBOPT_MIXLENS option if present.
    *-------------------------------------------------------------*/
   if (_mm._mixlens != NULL) {

      count[0] = nmats; 
      DBWriteComponent(dbfile, obj, "mixlens", name, "integer", _mm._mixlens,
                       1, count);
   }

   /*-------------------------------------------------------------
    *  Add the DBOPT_MATCOUNTS and DBOPT_MATLISTS options if present.
    *-------------------------------------------------------------*/
   if (_mm._matcounts != NULL && _mm._matlists != NULL) {

      count[0] = nmats; 
      DBWriteComponent(dbfile, obj, "matcounts", name, "integer", _mm._matcounts,
                       1, count);
      count[0] = 0;
      for (i = 0; i < nmats; i++)
         count[0] += _mm._matcounts[i];
      DBWriteComponent(dbfile, obj, "matlists", name, "integer", _mm._matlists,
                       1, count);
   }

   /*-------------------------------------------------------------
    *  Write multi-material object to SILO file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   FREE(tmp);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 * Routine                                     db_pdb_PutMultimatspecies
 *
 * Purpose
 *
 *    Write a multi-species object into the open SILO file.
 *
 * Programmer
 *    Jeremy S. Meredith
 *    Sept 17 1998
 *
 * Notes
 *
 * Modified
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added ngroups, blockorigin, and grouporigin.
 *
 *    Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *    Cast a strlen to long to avoid a compiler warning.
 *
 *    Jeremy Meredith, Thu Feb 10 16:00:53 PST 2000
 *    Fixed an off-by-one error on the max number of components.
 *
 *    Hank Childs, Wed Apr 11 08:05:24 PDT 2001
 *    Concatenate strings more intelligently [HYPer02535].
 *
 *    Hank Childs, Mon May 14 14:27:29 PDT 2001
 *    Fixed bug where there was an assumption that the string is
 *    NULL terminated.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutMultimatspecies (DBfile *dbfile, char *name, int nspec,
                    char *specnames[], DBoptlist *optlist) {

   int            i, len;
   long           count[3];
   char          *tmp = NULL, *cur = NULL;
   DBobject      *obj;

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   db_InitMulti(dbfile, optlist);

   /*-------------------------------------------------------------
    *  Build object description from literals and var-id's
    *-------------------------------------------------------------*/
   obj = DBMakeObject(name, DB_MULTIMATSPECIES, 13);
   DBAddIntComponent(obj, "nspec", nspec);
   DBAddIntComponent(obj, "ngroups", _mm._ngroups);
   DBAddIntComponent(obj, "blockorigin", _mm._blockorigin);
   DBAddIntComponent(obj, "grouporigin", _mm._grouporigin);
   if (_mm._guihide)
      DBAddIntComponent(obj, "guihide", _mm._guihide);

   /*-------------------------------------------------------------
    *  Define and write species before adding them to object.
    *-------------------------------------------------------------*/

    /* Compute size needed for string of concatenated block names.
     *
     * Note that we start with 2 so that we have one `;' and the NULL
     * terminator.  Also, the +1 in the "len +=" line is for the `;'
     * character.
     */
    len = 2;
    for(i=0; i<nspec; i++)
    {
        len += strlen(specnames[i]) + 1;
    }
    tmp = ALLOC_N(char,len);

   /* Build 1-D character string from 2-D mesh-name array */
   tmp[0] = ';';
   tmp[1] = '\0';

   cur = tmp+1;
   for (i = 0; i < nspec; i++) {
      int len2;
      len2 = strlen(specnames[i]);
      strncpy(cur, specnames[i], len2);
      cur += len2;
      strncpy(cur, ";", 1);
      cur += 1;
   }

   count[0] = (long) (cur - tmp);
   DBWriteComponent(dbfile, obj, "specnames", name, "char",
                    tmp, 1, count);

   /*-------------------------------------------------------------
    *  Define and write the time and cycle.
    *-------------------------------------------------------------*/
   DBAddIntComponent(obj, "cycle", _mm._cycle);

   if (_mm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _mm._nm_time);
   if (_mm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _mm._nm_dtime);

   /*-------------------------------------------------------------
    *  Write the DBOPT_MATNAME, _NMAT, and _NMATSPEC to the file.
    *-------------------------------------------------------------*/
   if (_mm._matname != NULL)
      DBAddStrComponent(obj, "matname", _mm._matname);

   if (_mm._nmat > 0 && _mm._nmatspec != NULL) {
      DBAddIntComponent(obj, "nmat", _mm._nmat);

      count[0]=_mm._nmat;
      DBWriteComponent(dbfile, obj, "nmatspec", name, "integer", _mm._nmatspec,
                       1, count);
   }
   /*-------------------------------------------------------------
    *  Write multi-species object to SILO file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   FREE(tmp);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                         db_pdb_PutPointmesh
 *
 *  Purpose
 *
 *      Write a point mesh object into the open output file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *      Al Leibee, Mon Apr 18 07:45:58 PDT 1994
 *      Added _dtime.
 *
 *      Robb Matzke, Fri Dec 2 14:16:41 PST 1994
 *      Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *      Robb Matzke, Sun Dec 18 17:07:34 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString and
 *      removed associated memory leak.
 *
 *      Sean Ahern, Sun Oct  1 03:15:18 PDT 1995
 *      Made "me" static.
 *
 *      Robb Matzke, 11 Nov 1997
 *      Added `datatype' to the file.
 *
 *      Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *      Added group_no.
 *
 *      Eric Brugger, Fri Mar  8 12:23:35 PST 2002
 *      I modified the routine to output min and max extents as either
 *      float or doubles, depending on the datatype of the coordinates.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutPointmesh (DBfile *dbfile, char *name, int ndims, float *coords[],
                     int nels, int datatype, DBoptlist *optlist) {

   int            i;
   long           count[3];
   DBobject      *obj;
   char          *datatype_str, tmp[1024];
   float          fmin_extents[3], fmax_extents[3];
   double         dmin_extents[3], dmax_extents[3];
   static char   *me = "db_pdb_PutPointmesh";

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   db_InitPoint(dbfile, optlist, ndims, nels);
   obj = DBMakeObject(name, DB_POINTMESH, 27);

   /*-------------------------------------------------------------
    *  Write coordinate arrays.
    *-------------------------------------------------------------*/
   datatype_str = db_GetDatatypeString(datatype);
   count[0] = nels;
   for (i = 0; i < ndims; i++) {
      sprintf(tmp, "coord%d", i);
      DBWriteComponent(dbfile, obj, tmp, name, datatype_str,
                       coords[i], 1, count);
   }
   FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Find the mesh extents from the coordinate arrays. Write
    *  them out to output file.
    *-------------------------------------------------------------*/
   count[0] = ndims;
   switch (datatype) {

   case DB_FLOAT:
      switch (ndims) {
      case 3:
         _DBarrminmax(coords[2], nels, &fmin_extents[2], &fmax_extents[2]);
      case 2:
         _DBarrminmax(coords[1], nels, &fmin_extents[1], &fmax_extents[1]);
      case 1:
         _DBarrminmax(coords[0], nels, &fmin_extents[0], &fmax_extents[0]);
         break;
      default:
         return db_perror("ndims", E_BADARGS, me);
      }

      DBWriteComponent(dbfile, obj, "min_extents", name, "float",
                       fmin_extents, 1, count);
      DBWriteComponent(dbfile, obj, "max_extents", name, "float",
                       fmax_extents, 1, count);

      break;
   case DB_DOUBLE:
      switch (ndims) {
      case 3:
         _DBdarrminmax((double *)coords[2], nels,
                       &dmin_extents[2], &dmax_extents[2]);
      case 2:
         _DBdarrminmax((double *)coords[1], nels,
                       &dmin_extents[1], &dmax_extents[1]);
      case 1:
         _DBdarrminmax((double *)coords[0], nels,
                       &dmin_extents[0], &dmax_extents[0]);
         break;
      default:
         return db_perror("ndims", E_BADARGS, me);
      }

      DBWriteComponent(dbfile, obj, "min_extents", name, "double",
                       dmin_extents, 1, count);
      DBWriteComponent(dbfile, obj, "max_extents", name, "double",
                       dmax_extents, 1, count);

      break;
   default:
      return db_perror("type not supported", E_NOTIMP, me);
   }

   if (_pm._gnodeno)
   {
       count[0] = nels;
       DBWriteComponent(dbfile, obj, "gnodeno", name, "integer",
                         _pm._gnodeno, 1, count);
   }

   /*-------------------------------------------------------------
    *  Build a SILO object definition for a point mesh. The minimum
    *  required information for a point mesh is the coordinate arrays,
    *  the number of dimensions, and the type (RECT/CURV). Process
    *  the provided options to complete the definition.
    *
    *  The SILO object definition is composed of a string of delimited
    *  component names plus an array of SILO *identifiers* of
    *  previously defined variables.
    *-------------------------------------------------------------*/
   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nspace", _pm._nspace);
   DBAddIntComponent(obj, "nels", _pm._nels);
   DBAddIntComponent(obj, "cycle", _pm._cycle);
   DBAddIntComponent(obj, "origin", _pm._origin);
   DBAddIntComponent(obj, "min_index", _pm._minindex);
   DBAddIntComponent(obj, "max_index", _pm._maxindex);
   DBAddIntComponent(obj, "datatype", datatype);
   if (_pm._guihide)
       DBAddIntComponent(obj, "guihide", _pm._guihide);
   if (_pm._group_no >= 0)
       DBAddIntComponent(obj, "group_no", _pm._group_no);
   if (_pm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _pm._nm_time);
   if (_pm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _pm._nm_dtime);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for x, y, &/or z,
    *-------------------------------------------------------------*/
   if (_pm._labels[0] != NULL)
      DBAddStrComponent(obj, "label0", _pm._labels[0]);

   if (_pm._labels[1] != NULL)
      DBAddStrComponent(obj, "label1", _pm._labels[1]);

   if (_pm._labels[2] != NULL)
      DBAddStrComponent(obj, "label2", _pm._labels[2]);

   if (_pm._units[0] != NULL)
      DBAddStrComponent(obj, "units0", _pm._units[0]);

   if (_pm._units[1] != NULL)
      DBAddStrComponent(obj, "units1", _pm._units[1]);

   if (_pm._units[2] != NULL)
      DBAddStrComponent(obj, "units2", _pm._units[2]);

   /*-------------------------------------------------------------
    *  Write point-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return (OKAY);
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_PutPointvar
 *
 *  Purpose
 *
 *      Write a point variable object into the open output file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *      Robb Matzke, Sun Dec 18 17:08:42 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString and removed
 *      associated memory leak.
 *
 *      Robb Matzke, Fri Dec 2 14:17:15 PST 1994
 *      Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *      Al Leibee, Mon Apr 18 07:45:58 PDT 1994
 *      Added _dtime.
 *
 *      Sean Ahern, Tue Mar 24 17:23:04 PST 1998
 *      Bumped up the min number of attributes so that we can write out 3D
 *      point meshes.
 *
 *      Mark C. Miller, Tue Sep  6 11:05:58 PDT 2005
 *      Removed duplicate DBAddStr call for "meshid"
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutPointvar (DBfile *dbfile, char *name, char *meshname, int nvars,
                    float **vars, int nels, int datatype,
                    DBoptlist *optlist) {

   int            i;
   long           count[3];
   DBobject      *obj;
   char          *datatype_str;
   char           tmp[1024];

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
#if 1                      /*Which one is correct? (1st one is the original) */
   db_InitPoint(dbfile, optlist, _pm._ndims, nels);
#else
   db_InitPoint(dbfile, optlist, ndims, nels);
#endif

   obj = DBMakeObject(name, DB_POINTVAR, 21);

   /*-------------------------------------------------------------
    *  Write variable arrays.
    *  Set index variables and counters.
    *-----------------------------------------------------------*/
   datatype_str = db_GetDatatypeString(datatype);
   count[0] = nels;
   if (nvars == 1) {
      DBWriteComponent(dbfile, obj, "_data", name, datatype_str,
                       vars[0], 1, count);
   }
   else {
      for (i = 0; i < nvars; i++) {
         sprintf(tmp, "%d_data", i);
         DBWriteComponent(dbfile, obj, tmp, name, datatype_str,
                          vars[i], 1, count);
      }
   }
   FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Build a SILO object definition for a point var. The
    *  minimum required information for a point var is the variable
    *  itself, plus the ID for the associated point mesh object.
    *  Process any additional options to complete the definition.
    *-------------------------------------------------------------*/

   DBAddStrComponent(obj, "meshid", meshname);
   if (_pm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _pm._nm_time);
   if (_pm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _pm._nm_dtime);

   DBAddIntComponent(obj, "nvals", nvars);
   DBAddIntComponent(obj, "nels", nels);
   DBAddIntComponent(obj, "ndims", 1);
   DBAddIntComponent(obj, "datatype", datatype);
   DBAddIntComponent(obj, "nspace", _pm._nspace);
   DBAddIntComponent(obj, "origin", _pm._origin);
   DBAddIntComponent(obj, "cycle", _pm._cycle);
   DBAddIntComponent(obj, "min_index", _pm._minindex);
   DBAddIntComponent(obj, "max_index", _pm._maxindex);
   if (_pm._guihide)
      DBAddIntComponent(obj, "guihide", _pm._guihide);
   if (_pm._ascii_labels)
       DBAddIntComponent(obj, "ascii_labels", _pm._ascii_labels);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for variable.
    *-------------------------------------------------------------*/
   if (_pm._label != NULL)
      DBAddStrComponent(obj, "label", _pm._label);

   if (_pm._unit != NULL)
      DBAddStrComponent(obj, "units", _pm._unit);

   /*-------------------------------------------------------------
    *  Write point-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, 0);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                           db_pdb_PutQuadmesh
 *
 *  Purpose
 *
 *      Write a quad mesh object into the open SILO file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *     Robb Matzke, Sun Dec 18 17:10:00 EST 1994
 *     Changed SW_GetDatatypeString to db_GetDatatypeString and removed
 *     associated memory leak.
 *
 *     Robb Matzke, Fri Dec 2 14:17:56 PST 1994
 *     Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *     Robb Matzke, Wed Nov 23 12:00:20 EST 1994
 *     Increased third argument of DBMakeObject from 25 to 40 because
 *     we were running out of room.  Thanks to the new error mechanism,
 *     we actually got a message for this!
 *
 *     Al Leibee, Sun Apr 17 07:54:25 PDT 1994
 *     Added dtime.
 *
 *     Sean Ahern, Fri Oct 16 17:48:23 PDT 1998
 *     Reformatted whitespace.
 *
 *     Sean Ahern, Tue Oct 20 17:22:58 PDT 1998
 *     Changed the extents so that they are written out in the datatype that
 *     is passed to the call.
 *
 *     Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *     Added group_no and base_index[].
 *
 *     Hank Childs, Thu Jan  6 13:48:40 PST 2000
 *     Put in lint directive for unused argument.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
/* ARGSUSED */
CALLBACK int
db_pdb_PutQuadmesh (DBfile *dbfile, char *name, char *coordnames[],
                    float *coords[], int dims[], int ndims, int datatype,
                    int coordtype, DBoptlist *optlist)
{
    int             i;
    long            count[3];
    int             nd;
    char           *datatype_str;
    DBobject       *obj;
    char            tmp[1024];

    /* The following is declared as double for worst case. */
    double         min_extents[3], max_extents[3];

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
    db_InitQuad(dbfile, name, optlist, dims, ndims);
    obj = DBMakeObject(name, coordtype, 43);

   /*-------------------------------------------------------------
    *  Write coordinate arrays.
    *-------------------------------------------------------------*/
    for (i = 0; i < ndims; i++)
        count[i] = dims[i];
    if (coordtype == DB_COLLINEAR)
        nd = 1;
    else
        nd = ndims;

    datatype_str = db_GetDatatypeString(datatype);
    for (i = 0; i < ndims; i++)
    {
        if (coordtype == DB_COLLINEAR)
            count[0] = dims[i];

        /* Do coordinate array, in form: coordN, where n = 0, 1, ... */
        sprintf(tmp, "coord%d", i);
        DBWriteComponent(dbfile, obj, tmp, name, datatype_str,
                         coords[i], nd, count);
    }

   /*-------------------------------------------------------------
    *  Find the mesh extents from the coordinate arrays. Write
    *  them out to SILO file.
    *-------------------------------------------------------------*/

    _DBQMCalcExtents(coords, datatype, _qm._minindex, _qm._maxindex_n, dims,
                     ndims, coordtype, min_extents, max_extents);

    count[0] = ndims;
    DBWriteComponent(dbfile, obj, "min_extents", name, datatype_str,
                     min_extents, 1, count);

    DBWriteComponent(dbfile, obj, "max_extents", name, datatype_str,
                     max_extents, 1, count);
    FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Build a SILO object definition for a quad mesh. The minimum
    *  required information for a quad mesh is the coordinate arrays,
    *  the number of dimensions, and the type (RECT/CURV). Process
    *  the provided options to complete the definition.
    *
    *  The SILO object definition is composed of a string of delimited
    *  component names plus an array of internal PDB variable names.
    *-------------------------------------------------------------*/

    DBAddIntComponent(obj, "ndims", ndims);
    DBAddIntComponent(obj, "coordtype", coordtype);
    DBAddIntComponent(obj, "datatype", datatype);
    DBAddIntComponent(obj, "nspace", _qm._nspace);
    DBAddIntComponent(obj, "nnodes", _qm._nnodes);
    DBAddIntComponent(obj, "facetype", _qm._facetype);
    DBAddIntComponent(obj, "major_order", _qm._majororder);
    DBAddIntComponent(obj, "cycle", _qm._cycle);
    DBAddIntComponent(obj, "coord_sys", _qm._coordsys);
    DBAddIntComponent(obj, "planar", _qm._planar);
    DBAddIntComponent(obj, "origin", _qm._origin);

   if (_qm._group_no >= 0)
       DBAddIntComponent(obj, "group_no", _qm._group_no);

    DBAddVarComponent(obj, "dims", _qm._nm_dims);
    DBAddVarComponent(obj, "min_index", _qm._nm_minindex);
    DBAddVarComponent(obj, "max_index", _qm._nm_maxindex_n);
    DBAddVarComponent(obj, "baseindex", _qm._nm_baseindex);

    if (_qm._time_set == TRUE)
        DBAddVarComponent(obj, "time", _qm._nm_time);
    if (_qm._dtime_set == TRUE)
        DBAddVarComponent(obj, "dtime", _qm._nm_dtime);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for x, y, &/or z,
    *-------------------------------------------------------------*/
    if (_qm._labels[0] != NULL)
        DBAddStrComponent(obj, "label0", _qm._labels[0]);

    if (_qm._labels[1] != NULL)
        DBAddStrComponent(obj, "label1", _qm._labels[1]);

    if (_qm._labels[2] != NULL)
        DBAddStrComponent(obj, "label2", _qm._labels[2]);

    if (_qm._units[0] != NULL)
        DBAddStrComponent(obj, "units0", _qm._units[0]);

    if (_qm._units[1] != NULL)
        DBAddStrComponent(obj, "units1", _qm._units[1]);

    if (_qm._units[2] != NULL)
        DBAddStrComponent(obj, "units2", _qm._units[2]);

    if (_qm._guihide)
        DBAddIntComponent(obj, "guihide", _qm._guihide);

   /*-------------------------------------------------------------
    *  Write quad-mesh object to SILO file.
    *-------------------------------------------------------------*/
    DBWriteObject(dbfile, obj, TRUE);
    DBFreeObject(obj);

    return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                            db_pdb_PutQuadvar
 *
 *  Purpose
 *
 *      Write a quad variable object into the open SILO file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *      Al Leibee, Sun Apr 17 07:54:25 PDT 1994
 *      Added dtime.
 *
 *      Al Leibee, Wed Jul 20 07:56:37 PDT 1994
 *      Added write of mixlen component.
 *
 *      Al Leibee, Wed Aug  3 16:57:38 PDT 1994
 *      Added _use_specmf.
 *
 *      Robb Matzke, Fri Dec 2 14:18:34 PST 1994
 *      Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *      Robb Matzke, Sun Dec 18 17:11:12 EST 1994
 *      Changed SW_GetDatatypeString to db_GetDatatypeString and
 *      removed associated memory leak.
 *
 *      Sean Ahern, Sun Oct  1 03:16:19 PDT 1995
 *      Made "me" static.
 *
 *      Robb Matzke, 1 May 1996
 *      The `dims' is actually used.  Before we just used the
 *      dimension variable whose name was based on the centering.  Now
 *      we store the dimensions for every quadvar.
 *
 *      Robb Matzke, 19 Jun 1997
 *      Added the `ascii_labels' optional field.  The default is
 *      false, so we write `ascii_labels' to the file only if it isn't
 *      false.
 *
 *      Eric Brugger, Mon Oct  6 15:11:26 PDT 1997
 *      I modified the routine to output the maximum index properly.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutQuadvar (DBfile *_dbfile, char *name, char *meshname, int nvars,
                   char *varnames[], float *vars[], int dims[], int ndims,
                   float *mixvars[], int mixlen, int datatype, int centering,
                   DBoptlist *optlist) {

   DBfile_pdb   *dbfile = (DBfile_pdb *) _dbfile;
   int          i, nels;
   long         count[3], mcount[1];
   char         *suffix, *datatype_str, tmp1[1024], tmp2[1024];
   static char  *me = "db_pdb_PutQuadvar";
   DBobject     *obj;
   int          maxindex[3] ;

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   db_InitQuad(_dbfile, meshname, optlist, dims, ndims);

   obj = DBMakeObject(name, DB_QUADVAR, 26);

   DBAddStrComponent(obj, "meshid", meshname);

   /*-------------------------------------------------------------
    *  Write variable arrays.
    *  Set index variables and counters.
    *-----------------------------------------------------------*/
   nels = 1;
   for (i = 0; i < ndims; i++) {
      count[i] = dims[i];
      nels *= dims[i];
   }

   switch (centering) {
   case DB_NODECENT:
      DBAddVarComponent(obj, "align", _qm._nm_alignn);
      break;

   case DB_ZONECENT:
   case DB_FACECENT:
      DBAddVarComponent(obj, "align", _qm._nm_alignz);
      break;

   default:
      return db_perror("centering", E_BADARGS, me);
   }

   /*
    * Dimensions
    */
   db_mkname (dbfile->pdb, name, "dims", tmp2) ;
   mcount[0] = ndims ;
   PJ_write_len (dbfile->pdb, tmp2, "integer", dims, 1, mcount) ;
   DBAddVarComponent (obj, "dims", tmp2) ;

   /*
    * Max indices
    */
   for (i=0; i<ndims; i++) maxindex[i] = dims[i] - _qm._hi_offset[i] - 1 ;
   db_mkname (dbfile->pdb, name, "maxindex", tmp2) ;
   mcount[0] = ndims ;
   PJ_write_len (dbfile->pdb, tmp2, "integer", maxindex, 1, mcount) ;
   DBAddVarComponent (obj, "max_index", tmp2) ;

   /*-------------------------------------------------------------
    *  We first will write the given variables to SILO, then
    *  we'll define a Quadvar object in SILO composed of the
    *  variables plus the given options.
    *-------------------------------------------------------------*/

   suffix = "data";

   datatype_str = db_GetDatatypeString(datatype);
   for (i = 0; i < nvars; i++) {

      db_mkname(dbfile->pdb, varnames[i], suffix, tmp2);
      PJ_write_len(dbfile->pdb, tmp2, datatype_str, vars[i],
                   ndims, count);

      sprintf(tmp1, "value%d", i);
      DBAddVarComponent(obj, tmp1, tmp2);

      /* Write the mixed data component if present */
      if (mixvars != NULL && mixvars[i] != NULL && mixlen > 0) {
         mcount[0] = mixlen;

         db_mkname(dbfile->pdb, varnames[i], "mix", tmp2);
         PJ_write_len(dbfile->pdb, tmp2, datatype_str, mixvars[i],
                      1, mcount);

         sprintf(tmp1, "mixed_value%d", i);
         DBAddVarComponent(obj, tmp1, tmp2);
      }
   }
   FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Build a SILO object definition for a quad mesh var. The
    *  minimum required information for a quad var is the variable
    *  itself, plus the ID for the associated quad mesh object.
    *  Process any additional options to complete the definition.
    *-------------------------------------------------------------*/

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nvals", nvars);
   DBAddIntComponent(obj, "nels", nels);
   DBAddIntComponent(obj, "origin", _qm._origin);
   DBAddIntComponent(obj, "datatype", datatype);
   DBAddIntComponent(obj, "mixlen", mixlen);

   /*-------------------------------------------------------------
    * Add 'recommended' optional components.
    *-------------------------------------------------------------*/
   DBAddIntComponent(obj, "major_order", _qm._majororder);
   DBAddIntComponent(obj, "cycle", _qm._cycle);

   if (_qm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _qm._nm_time);
   if (_qm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _qm._nm_dtime);

   DBAddVarComponent(obj, "min_index", _qm._nm_minindex);
   DBAddIntComponent(obj, "use_specmf", _qm._use_specmf);
   if (_qm._ascii_labels) {
      DBAddIntComponent(obj, "ascii_labels", _qm._ascii_labels);
   }

   if (_qm._guihide)
      DBAddIntComponent(obj, "guihide", _qm._guihide);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for variable.
    *-------------------------------------------------------------*/
   if (_qm._label != NULL)
      DBAddStrComponent(obj, "label", _qm._label);

   if (_qm._unit != NULL)
      DBAddStrComponent(obj, "units", _qm._unit);

   /*-------------------------------------------------------------
    *  Write quad-var object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(_dbfile, obj, 0);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                           db_pdb_PutCsgmesh
 *
 *  Purpose
 *
 *      Write a csg mesh object into the open output file.
 *
 *  Programmer
 *
 *      Mark C. Miller , Tue Aug  2 19:15:39 PDT 2005
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
/* ARGSUSED */
CALLBACK int
db_pdb_PutCsgmesh (DBfile *dbfile, const char *name, int ndims,
                   int nbounds,
                   const int *typeflags, const int *bndids,
                   const void *coeffs, int lcoeffs, int datatype,
                   const double *extents, const char *zlname,
                   DBoptlist *optlist) {

   int            i;
   long           count[3];
   DBobject      *obj;
   char          *datatype_str;
   char           tmp[256];
   double         min_extents[3], max_extents[3];

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   strcpy(_csgm._meshname, name);

   db_InitCsg(dbfile, (char*) name, optlist);

   obj = DBMakeObject(name, DB_CSGMESH, 31);

   count[0] = nbounds;

   DBWriteComponent(dbfile, obj, "typeflags", name, "integer",
                    typeflags, 1, count);

   if (bndids)
       DBWriteComponent(dbfile, obj, "bndids", name, "integer",
                        bndids, 1, count);

   datatype_str = db_GetDatatypeString(datatype);
   count[0] = lcoeffs;
   DBWriteComponent(dbfile, obj, "coeffs", name, datatype_str,
                       coeffs, 1, count);
   FREE(datatype_str);

   min_extents[0] = extents[0];
   min_extents[1] = extents[1];
   min_extents[2] = extents[2];
   max_extents[0] = extents[3];
   max_extents[1] = extents[4];
   max_extents[2] = extents[5];

   count[0] = ndims;
   DBWriteComponent(dbfile, obj, "min_extents", name, "double",
                    min_extents, 1, count);
   DBWriteComponent(dbfile, obj, "max_extents", name, "double",
                    max_extents, 1, count);

   /*-------------------------------------------------------------
    *  Build a output object definition for a ucd mesh. The minimum
    *  required information for a ucd mesh is the coordinate arrays,
    *  the number of dimensions, and the type (RECT/CURV). Process
    *  the provided options to complete the definition.
    *
    *  The output object definition is composed of a string of delimited
    *  component names plus an array of output *identifiers* of
    *  previously defined variables.
    *-------------------------------------------------------------*/

   if (zlname)
      DBAddStrComponent(obj, "csgzonelist", zlname);

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nbounds", nbounds);
   DBAddIntComponent(obj, "cycle", _csgm._cycle);
   DBAddIntComponent(obj, "datatype", datatype);
   DBAddIntComponent(obj, "lcoeffs", lcoeffs);
   if (_csgm._guihide)
       DBAddIntComponent(obj, "guihide", _csgm._guihide);

   if (_csgm._group_no >= 0)
       DBAddIntComponent(obj, "group_no", _csgm._group_no);

   if (_csgm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _csgm._nm_time);
   if (_csgm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _csgm._nm_dtime);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for x, y, &/or z,
    *-------------------------------------------------------------*/
   if (_csgm._labels[0] != NULL)
      DBAddStrComponent(obj, "label0", _csgm._labels[0]);

   if (_csgm._labels[1] != NULL)
      DBAddStrComponent(obj, "label1", _csgm._labels[1]);

   if (_csgm._labels[2] != NULL)
      DBAddStrComponent(obj, "label2", _csgm._labels[2]);

   if (_csgm._units[0] != NULL)
      DBAddStrComponent(obj, "units0", _csgm._units[0]);

   if (_csgm._units[1] != NULL)
      DBAddStrComponent(obj, "units1", _csgm._units[1]);

   if (_csgm._units[2] != NULL)
      DBAddStrComponent(obj, "units2", _csgm._units[2]);

   /*-------------------------------------------------------------
    *  Write csg-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                            db_pdb_PutCsgvar
 *
 *  Purpose
 *
 *      Write a csg variable object into the open output file.
 *
 *  Programmer
 *
 *      Mark C. Miller, August 10. 2005 
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutCsgvar (DBfile *_dbfile, const char *name, const char *meshname,
                  int nvars, const char *varnames[], const void *vars[],
                  int nels, int datatype, int centering,
                  DBoptlist *optlist) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   int            i;
   long           count[3], mcount[1];
   DBobject      *obj;
   char          *suffix, *datatype_str, tmp1[256], tmp2[256];
   static char   *me = "db_pdb_PutCsgvar";

   db_InitCsg(_dbfile, (char*) name, optlist);

   obj = DBMakeObject(name, DB_CSGVAR, 26);

   DBAddStrComponent(obj, "meshid", meshname);

   /*-------------------------------------------------------------
    *  Write variable arrays.
    *  Set index variables and counters.
    *-----------------------------------------------------------*/

   count[0] = nels;

   switch (centering) {
   case DB_NODECENT:
   case DB_ZONECENT:
   case DB_FACECENT:
      break;

   default:
      return db_perror("centering", E_BADARGS, me);
   }

   /*-------------------------------------------------------------
    *  We first will write the given variables to output, then
    *  we'll define a Ucdvar object in output composed of the
    *  variables plus the given options.
    *-------------------------------------------------------------*/

   suffix = "data";
   datatype_str = db_GetDatatypeString(datatype);

   for (i = 0; i < nvars; i++) {

      db_mkname(dbfile->pdb, (char*) varnames[i], suffix, tmp2);
      PJ_write_len(dbfile->pdb, tmp2, datatype_str, vars[i],
                   1, count);

      sprintf(tmp1, "value%d", i);
      DBAddVarComponent(obj, tmp1, tmp2);

   }
   FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Build a output object definition for a ucd mesh var. The
    *  minimum required information for a ucd var is the variable
    *  itself, plus the ID for the associated ucd mesh object.
    *  Process any additional options to complete the definition.
    *-------------------------------------------------------------*/

   DBAddIntComponent(obj, "nvals", nvars);
   DBAddIntComponent(obj, "nels", nels);
   DBAddIntComponent(obj, "centering", centering);
   DBAddIntComponent(obj, "datatype", datatype);
   if (_csgm._guihide)
      DBAddIntComponent(obj, "guihide", _csgm._guihide);

   /*-------------------------------------------------------------
    * Add 'recommended' optional components.
    *-------------------------------------------------------------*/
   if (_csgm._time_set == TRUE)
      DBAddVarComponent(obj, "time", _csgm._nm_time);
   if (_csgm._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _csgm._nm_dtime);

   if (centering == DB_ZONECENT)
   {
      if (_csgm._hi_offset_set == TRUE)
         DBAddIntComponent(obj, "hi_offset", _csgm._hi_offset);
      if (_csgm._lo_offset_set == TRUE)
         DBAddIntComponent(obj, "lo_offset", _csgm._lo_offset);
   }

   DBAddIntComponent(obj, "cycle", _csgm._cycle);
   DBAddIntComponent(obj, "use_specmf", _csgm._use_specmf);
   if (_csgm._ascii_labels) {
      DBAddIntComponent(obj, "ascii_labels", _csgm._ascii_labels);
   }

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for variable.
    *-------------------------------------------------------------*/
   if (_csgm._label != NULL)
      DBAddStrComponent(obj, "label", _csgm._label);

   if (_csgm._unit != NULL)
      DBAddStrComponent(obj, "units", _csgm._unit);

   /*-------------------------------------------------------------
    *  Write ucd-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(_dbfile, obj, 0);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                        db_pdb_PutCSGZonelist
 *
 *  Purpose
 *
 *      Write a csg zonelist object into the open output file.
 *
 *  Programmer
 *
 *      Mark C. Miller 
 *      August 9, 2005 
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutCSGZonelist (DBfile *dbfile, const char *name, int nregs,
                 const int *typeflags,
                 const int *leftids, const int *rightids,
                 const void *xforms, int lxforms, int datatype,
                 int nzones, const int *zonelist, DBoptlist *optlist) {

   long           count[1];
   DBobject      *obj;

   memset(&_csgzl, 0, sizeof(_csgzl));
   db_ProcessOptlist(DB_CSGZONELIST, optlist);

   /*--------------------------------------------------
    *  Build up object description by defining literals
    *  and defining/writing arrays.
    *-------------------------------------------------*/
   obj = DBMakeObject(name, DB_CSGZONELIST, 15);

   DBAddIntComponent(obj, "nregs", nregs);
   DBAddIntComponent(obj, "datatype", datatype);
   DBAddIntComponent(obj, "nzones", nzones);

   count[0] = nregs;
   DBWriteComponent(dbfile, obj, "typeflags", name, "integer",
                    typeflags, 1, count);
   DBWriteComponent(dbfile, obj, "leftids", name, "integer",
                    leftids, 1, count);
   DBWriteComponent(dbfile, obj, "rightids", name, "integer",
                    rightids, 1, count);
   count[0] = nzones;
   DBWriteComponent(dbfile, obj, "zonelist", name, "integer",
                    zonelist, 1, count);

   if (xforms && lxforms > 0)
   {
       char *datatype_str = db_GetDatatypeString(datatype);
       count[0] = lxforms;
       DBWriteComponent(dbfile, obj, "xforms", name, datatype_str,
                           xforms, 1, count);
   }

   if (_csgzl._regnames)
   {
       int len; char *tmp;
       db_StringArrayToStringList((const char**) _csgzl._regnames, nregs, &tmp, &len);
       count[0] = len;
       DBWriteComponent(dbfile, obj, "regnames", name, "char",
                        tmp, 1, count);
       FREE(tmp);
   }

   if (_csgzl._zonenames)
   {
       int len; char *tmp;
       db_StringArrayToStringList((const char**) _csgzl._zonenames, nzones, &tmp, &len);
       count[0] = len;
       DBWriteComponent(dbfile, obj, "zonenames", name, "char",
                        tmp, 1, count);
       FREE(tmp);
   }

   /*-------------------------------------------------------------
    *  Write object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                           db_pdb_PutUcdmesh
 *
 *  Purpose
 *
 *      Write a ucd mesh object into the open output file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *     Robb Matzke, Sun Dec 18 17:12:16 EST 1994
 *     Changed SW_GetDatatypeString to db_GetDatatypeString and
 *     removed associated memory leak.
 *
 *     Robb Matzke, Fri Dec 2 14:19:04 PST 1994
 *     Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *     Al Leibee, Mon Apr 18 07:45:58 PDT 1994
 *     Added _dtime.
 *
 *     Sean Ahern, Wed Oct 21 09:22:30 PDT 1998
 *     Changed how we pass extents data to the UM_CalcExtents function.
 *
 *     Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *     Added group_no and gnodeno.
 *
 *     Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *     Put in lint directive for unused arguments.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
/* ARGSUSED */
CALLBACK int
db_pdb_PutUcdmesh (DBfile *dbfile, char *name, int ndims, char *coordnames[],
                   float *coords[], int nnodes, int nzones, char *zlname,
                   char *flname, int datatype, DBoptlist *optlist) {

   int            i;
   long           count[3];
   DBobject      *obj;
   char          *datatype_str;
   char           tmp[256];

   /* Following is declared as double for worst case */
   double         min_extents[3], max_extents[3];

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
   strcpy(_um._meshname, name);

   db_InitUcd(dbfile, name, optlist, ndims, nnodes, nzones);

   obj = DBMakeObject(name, DB_UCDMESH, 29);

   /*-------------------------------------------------------------
    *  We first will write the given coordinate arrays to output,
    *  then we'll define a UCD-mesh object in output composed of the
    *  coordinates plus the given options.
    *-------------------------------------------------------------*/

   datatype_str = db_GetDatatypeString(datatype);
   count[0] = nnodes;

   for (i = 0; i < ndims; i++) {

      sprintf(tmp, "coord%d", i);

      DBWriteComponent(dbfile, obj, tmp, name, datatype_str,
                       coords[i], 1, count);
   }

   /*-------------------------------------------------------------
    *  Find the mesh extents from the coordinate arrays. Write
    *  them out to output file.
    *-------------------------------------------------------------*/

   UM_CalcExtents(coords, datatype, ndims, nnodes, min_extents, max_extents);

   count[0] = ndims;
   DBWriteComponent(dbfile, obj, "min_extents", name, datatype_str,
                    min_extents, 1, count);

   DBWriteComponent(dbfile, obj, "max_extents", name, datatype_str,
                    max_extents, 1, count);
   FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Build a output object definition for a ucd mesh. The minimum
    *  required information for a ucd mesh is the coordinate arrays,
    *  the number of dimensions, and the type (RECT/CURV). Process
    *  the provided options to complete the definition.
    *
    *  The output object definition is composed of a string of delimited
    *  component names plus an array of output *identifiers* of
    *  previously defined variables.
    *-------------------------------------------------------------*/

   if (flname)
      DBAddStrComponent(obj, "facelist", flname);
   if (zlname)
      DBAddStrComponent(obj, "zonelist", zlname);

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nnodes", nnodes);
   DBAddIntComponent(obj, "nzones", nzones);
   DBAddIntComponent(obj, "facetype", _um._facetype);
   DBAddIntComponent(obj, "cycle", _um._cycle);
   DBAddIntComponent(obj, "coord_sys", _um._coordsys);
   DBAddIntComponent(obj, "topo_dim", _um._topo_dim);
   DBAddIntComponent(obj, "planar", _um._planar);
   DBAddIntComponent(obj, "origin", _um._origin);
   DBAddIntComponent(obj, "datatype", datatype);

   if (_um._gnodeno)
   {
       count[0] = nnodes;
       DBWriteComponent(dbfile, obj, "gnodeno", name, "integer",
                         _um._gnodeno, 1, count);
   }

   if (_um._group_no >= 0)
       DBAddIntComponent(obj, "group_no", _um._group_no);

   if (_um._time_set == TRUE)
      DBAddVarComponent(obj, "time", _um._nm_time);
   if (_um._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _um._nm_dtime);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for x, y, &/or z,
    *-------------------------------------------------------------*/
   if (_um._labels[0] != NULL)
      DBAddStrComponent(obj, "label0", _um._labels[0]);

   if (_um._labels[1] != NULL)
      DBAddStrComponent(obj, "label1", _um._labels[1]);

   if (_um._labels[2] != NULL)
      DBAddStrComponent(obj, "label2", _um._labels[2]);

   if (_um._units[0] != NULL)
      DBAddStrComponent(obj, "units0", _um._units[0]);

   if (_um._units[1] != NULL)
      DBAddStrComponent(obj, "units1", _um._units[1]);

   if (_um._units[2] != NULL)
      DBAddStrComponent(obj, "units2", _um._units[2]);

   if (_um._guihide)
      DBAddIntComponent(obj, "guihide", _um._guihide);

   /*-------------------------------------------------------------
    *  Optional polyhedral zonelist 
    *-------------------------------------------------------------*/
   if (_um._phzl_name != NULL)
      DBAddStrComponent(obj, "phzonelist", _um._phzl_name);

   /*-------------------------------------------------------------
    *  Write ucd-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                         db_pdb_PutUcdsubmesh
 *
 *  Purpose
 *
 *      Write a subset of a ucd mesh object into the open output file.
 *
 *  Programmer
 *
 *      Jim Reus
 *
 *  Notes
 *
 *  Modifications
 *     none yet.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutUcdsubmesh (DBfile *dbfile, char *name, char *parentmesh,
                      int nzones, char *zlname, char *flname,
                      DBoptlist *optlist) {

   int            i;
   DBobject      *obj;
   int           *Pdatatype,datatype;
   int           *Pndims,ndims;
   int           *Pnnodes,nnodes;

   /*-------------------------------------------------------------
    *  We first retreive certain attributes from the parent mesh.
    *  Of course this had better succeed...
    *-------------------------------------------------------------*/

   Pndims       = DBGetComponent(dbfile,parentmesh,"ndims");
          ndims = *Pndims;
   Pnnodes      = DBGetComponent(dbfile,parentmesh,"nnodes");
         nnodes = *Pnnodes;
   Pdatatype    = DBGetComponent(dbfile,parentmesh,"datatype");
       datatype = *Pdatatype;

   /*-------------------------------------------------------------
    *  Now we can initialize global data, and process options.
    *-------------------------------------------------------------*/

   strcpy(_um._meshname, name);

   db_InitUcd(dbfile, name, optlist, ndims, nnodes, nzones);

   obj = DBMakeObject(name, DB_UCDMESH, 26);

   /*-------------------------------------------------------------
    *  Then we will add references to the coordinate arrays of the
    *  parent mesh...
    *-------------------------------------------------------------*/

   for (i = 0; i < ndims; i++) {
      char           myComponName[256];
      char           parentComponName[256];

      sprintf(myComponName, "coord%d", i);
      sprintf(parentComponName, "%s_coord%d", parentmesh, i);
      DBAddVarComponent(obj,myComponName,parentComponName);
   }

   /*-------------------------------------------------------------
    *  For now we'll simply refer to the extents provided for the
    *  parent mesh.  It would be be better to compute extents for
    *  the cordinates actually used, but then we'd have to retrieve
    *  the coords...
    *-------------------------------------------------------------*/

   {  char           myComponName[256];
      char           parentComponName[256];

      sprintf(myComponName, "min_extents");
      sprintf(parentComponName, "%s_min_extents", parentmesh);
      DBAddVarComponent(obj,myComponName,parentComponName);

      sprintf(myComponName, "max_extents");
      sprintf(parentComponName, "%s_max_extents", parentmesh);
      DBAddVarComponent(obj,myComponName,parentComponName);
   }

   /*-------------------------------------------------------------
    *  Build a output object definition for a ucd mesh. The minimum
    *  required information for a ucd mesh is the coordinate arrays,
    *  the number of dimensions, and the type (RECT/CURV). Process
    *  the provided options to complete the definition.
    *
    *  The output object definition is composed of a string of delimited
    *  component names plus an array of output *identifiers* of
    *  previously defined variables.
    *-------------------------------------------------------------*/

   if (flname)
      DBAddStrComponent(obj, "facelist", flname);
   if (zlname)
      DBAddStrComponent(obj, "zonelist", zlname);

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nnodes", nnodes);
   DBAddIntComponent(obj, "nzones", nzones);
   DBAddIntComponent(obj, "facetype", _um._facetype);
   DBAddIntComponent(obj, "cycle", _um._cycle);
   DBAddIntComponent(obj, "coord_sys", _um._coordsys);
   DBAddIntComponent(obj, "topo_dim", _um._topo_dim);
   DBAddIntComponent(obj, "planar", _um._planar);
   DBAddIntComponent(obj, "origin", _um._origin);
   DBAddIntComponent(obj, "datatype", datatype);

   if (_um._time_set == TRUE)
      DBAddVarComponent(obj, "time", _um._nm_time);
   if (_um._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _um._nm_dtime);

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for x, y, &/or z,
    *-------------------------------------------------------------*/
   if (_um._labels[0] != NULL)
      DBAddStrComponent(obj, "label0", _um._labels[0]);

   if (_um._labels[1] != NULL)
      DBAddStrComponent(obj, "label1", _um._labels[1]);

   if (_um._labels[2] != NULL)
      DBAddStrComponent(obj, "label2", _um._labels[2]);

   if (_um._units[0] != NULL)
      DBAddStrComponent(obj, "units0", _um._units[0]);

   if (_um._units[1] != NULL)
      DBAddStrComponent(obj, "units1", _um._units[1]);

   if (_um._units[2] != NULL)
      DBAddStrComponent(obj, "units2", _um._units[2]);

   if (_um._guihide)
      DBAddIntComponent(obj, "guihide", _um._guihide);

   /*-------------------------------------------------------------
    *  Write ucd-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);

   FREE(Pdatatype);
   FREE(Pnnodes);
   FREE(Pndims);

   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */


/*----------------------------------------------------------------------
 *  Routine                                            db_pdb_PutUcdvar
 *
 *  Purpose
 *
 *      Write a ucd variable object into the open output file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *     Al Leibee, Mon Apr 18 07:45:58 PDT 1994
 *     Added _dtime.
 *
 *     Al Leibee, Mon Jul 11 12:00:00 PDT 1994
 *     Use mixlen, not _mixlen.
 *
 *     Al Leibee, Wed Jul 20 07:56:37 PDT 1994
 *     Write mixlen component.
 *
 *     Al Leibee, Wed Aug  3 16:57:38 PDT 1994
 *     Added _use_specmf.
 *
 *     Robb Matzke, Fri Dec 2 14:19:42 PST 1994
 *     Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *     Robb Matzke, Sun Dec 18 17:13:17 EST 1994
 *     Changed SW_GetDatatypeString to db_GetDatatypeString and
 *     removed associated memory leak.
 *
 *     Sean Ahern, Sun Oct  1 03:17:35 PDT 1995
 *     Made "me" static.
 *
 *     Eric Brugger, Wed Oct 15 14:45:47 PDT 1997
 *     Added _hi_offset and _lo_offset.
 *
 *     Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *     Removed unused variable nm_cent.
 *
 *     Robb Matzke, 2000-05-23
 *     Removed the duplicate `meshid' field.
 *
 *     Brad Whitlock, Wed Jan 18 15:09:57 PST 2006
 *     Added ascii_labels.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutUcdvar (DBfile *_dbfile, char *name, char *meshname, int nvars,
                  char *varnames[], float *vars[], int nels, float *mixvars[],
                  int mixlen, int datatype, int centering,
                  DBoptlist *optlist) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   int            i;
   long           count[3], mcount[1];
   DBobject      *obj;
   char          *suffix, *datatype_str, tmp1[256], tmp2[256];
   static char   *me = "db_pdb_PutUcdvar";

   /*-------------------------------------------------------------
    *  Initialize global data, and process options.
    *-------------------------------------------------------------*/
#if 1                       /*which way is right?  (1st one is the original) */
   db_InitUcd(_dbfile, meshname, optlist, _um._ndims,
              _um._nnodes, _um._nzones);
#else
   db_InitUcd(_dbfile, meshname, optlist, ndims, nnodes, nzones);
#endif

   obj = DBMakeObject(name, DB_UCDVAR, 26);

   DBAddStrComponent(obj, "meshid", meshname);

   /*-------------------------------------------------------------
    *  Write variable arrays.
    *  Set index variables and counters.
    *-----------------------------------------------------------*/

   count[0] = nels;

   switch (centering) {
   case DB_NODECENT:
   case DB_ZONECENT:
   case DB_FACECENT:
      break;

   default:
      return db_perror("centering", E_BADARGS, me);
   }

   /*-------------------------------------------------------------
    *  We first will write the given variables to output, then
    *  we'll define a Ucdvar object in output composed of the
    *  variables plus the given options.
    *-------------------------------------------------------------*/

   suffix = "data";
   datatype_str = db_GetDatatypeString(datatype);

   for (i = 0; i < nvars; i++) {

      db_mkname(dbfile->pdb, varnames[i], suffix, tmp2);
      PJ_write_len(dbfile->pdb, tmp2, datatype_str, vars[i],
                   1, count);

      sprintf(tmp1, "value%d", i);
      DBAddVarComponent(obj, tmp1, tmp2);

      /* Write the mixed data component if present */
      if (mixvars != NULL && mixvars[i] != NULL && mixlen > 0) {
         mcount[0] = mixlen;

         db_mkname(dbfile->pdb, varnames[i], "mix", tmp2);
         PJ_write_len(dbfile->pdb, tmp2, datatype_str, mixvars[i],
                      1, mcount);

         sprintf(tmp1, "mixed_value%d", i);
         DBAddVarComponent(obj, tmp1, tmp2);
      }
   }
   FREE(datatype_str);

   /*-------------------------------------------------------------
    *  Build a output object definition for a ucd mesh var. The
    *  minimum required information for a ucd var is the variable
    *  itself, plus the ID for the associated ucd mesh object.
    *  Process any additional options to complete the definition.
    *-------------------------------------------------------------*/

   DBAddIntComponent(obj, "ndims", _um._ndims);
   DBAddIntComponent(obj, "nvals", nvars);
   DBAddIntComponent(obj, "nels", nels);
   DBAddIntComponent(obj, "centering", centering);
   DBAddIntComponent(obj, "origin", _um._origin);
   DBAddIntComponent(obj, "mixlen", mixlen);
   DBAddIntComponent(obj, "datatype", datatype);

   /*-------------------------------------------------------------
    * Add 'recommended' optional components.
    *-------------------------------------------------------------*/
   if (_um._time_set == TRUE)
      DBAddVarComponent(obj, "time", _um._nm_time);
   if (_um._dtime_set == TRUE)
      DBAddVarComponent(obj, "dtime", _um._nm_dtime);

   if (centering == DB_ZONECENT)
   {
      if (_um._hi_offset_set == TRUE)
         DBAddIntComponent(obj, "hi_offset", _um._hi_offset);
      if (_um._lo_offset_set == TRUE)
         DBAddIntComponent(obj, "lo_offset", _um._lo_offset);
   }

   DBAddIntComponent(obj, "cycle", _um._cycle);
   DBAddIntComponent(obj, "use_specmf", _um._use_specmf);
   if (_um._ascii_labels) {
      DBAddIntComponent(obj, "ascii_labels", _um._ascii_labels);
   }

   /*-------------------------------------------------------------
    *  Process character strings: labels & units for variable.
    *-------------------------------------------------------------*/
   if (_um._label != NULL)
      DBAddStrComponent(obj, "label", _um._label);

   if (_um._unit != NULL)
      DBAddStrComponent(obj, "units", _um._unit);

   if (_um._guihide)
      DBAddIntComponent(obj, "guihide", _um._guihide);

   /*-------------------------------------------------------------
    *  Write ucd-mesh object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(_dbfile, obj, 0);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                           db_pdb_PutZonelist
 *
 *  Purpose
 *
 *      Write a ucd zonelist object into the open output file.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modified
 *     Robb Matzke, Fri Dec 2 14:20:10 PST 1994
 *     Changed SCFREE(obj) to DBFreeObject(obj)
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutZonelist (DBfile *dbfile, char *name, int nzones, int ndims,
                    int nodelist[], int lnodelist, int origin, int shapesize[],
                    int shapecnt[], int nshapes) {

   long           count[5];
   DBobject      *obj;

   /*--------------------------------------------------
    *  Build up object description by defining literals
    *  and defining/writing arrays.
    *-------------------------------------------------*/
   obj = DBMakeObject(name, DB_ZONELIST, 15);

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nzones", nzones);
   DBAddIntComponent(obj, "nshapes", nshapes);
   DBAddIntComponent(obj, "lnodelist", lnodelist);
   DBAddIntComponent(obj, "origin", origin);

   count[0] = lnodelist;

   DBWriteComponent(dbfile, obj, "nodelist", name, "integer",
                    nodelist, 1, count);

   count[0] = nshapes;

   DBWriteComponent(dbfile, obj, "shapecnt", name, "integer",
                    shapecnt, 1, count);

   DBWriteComponent(dbfile, obj, "shapesize", name, "integer",
                    shapesize, 1, count);

   /*-------------------------------------------------------------
    *  Write object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                          db_pdb_PutZonelist2
 *
 *  Purpose
 *
 *      Write a ucd zonelist object into the open output file.
 *
 *  Programmer
 *
 *      Eric Brugger
 *      March 30, 1999
 *
 *  Notes
 *
 *  Modified
 *
 *    Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *    Added an option list, a call to initialize it, and gzoneno.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutZonelist2 (DBfile *dbfile, char *name, int nzones, int ndims,
                     int nodelist[], int lnodelist, int origin,
                     int lo_offset, int hi_offset, int shapetype[],
                     int shapesize[], int shapecnt[], int nshapes,
                     DBoptlist *optlist) {

   long           count[5];
   DBobject      *obj;

   db_InitZonelist(dbfile, optlist);

   /*--------------------------------------------------
    *  Build up object description by defining literals
    *  and defining/writing arrays.
    *-------------------------------------------------*/
   obj = DBMakeObject(name, DB_ZONELIST, 15);

   DBAddIntComponent(obj, "ndims", ndims);
   DBAddIntComponent(obj, "nzones", nzones);
   DBAddIntComponent(obj, "nshapes", nshapes);
   DBAddIntComponent(obj, "lnodelist", lnodelist);
   DBAddIntComponent(obj, "origin", origin);
   DBAddIntComponent(obj, "lo_offset", lo_offset);
   DBAddIntComponent(obj, "hi_offset", hi_offset);

   count[0] = lnodelist;

   DBWriteComponent(dbfile, obj, "nodelist", name, "integer",
                    nodelist, 1, count);

   count[0] = nshapes;

   DBWriteComponent(dbfile, obj, "shapecnt", name, "integer",
                    shapecnt, 1, count);

   DBWriteComponent(dbfile, obj, "shapesize", name, "integer",
                    shapesize, 1, count);

   DBWriteComponent(dbfile, obj, "shapetype", name, "integer",
                    shapetype, 1, count);

   if (_uzl._gzoneno)
   {
       count[0] = nzones;
       DBWriteComponent(dbfile, obj, "gzoneno", name, "integer",
                        _uzl._gzoneno, 1, count);
   }

   /*-------------------------------------------------------------
    *  Write object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                 db_pdb_PutPHZonelist
 *
 *  Purpose
 *
 *      Write a polyhedral zonelist object into the open output file.
 *
 *  Programmer
 *
 *      Mark C. Miller 
 *      July 27, 2004
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
CALLBACK int
db_pdb_PutPHZonelist (DBfile *dbfile, char *name,
   int nfaces, int *nodecnt, int lnodelist, int *nodelist, char *extface,
   int nzones, int *facecnt, int lfacelist, int *facelist,
   int origin, int lo_offset, int hi_offset,
   DBoptlist *optlist) {

   long           count[1];
   DBobject      *obj;

   db_InitPHZonelist(dbfile, optlist);

   /*--------------------------------------------------
    *  Build up object description by defining literals
    *  and defining/writing arrays.
    *-------------------------------------------------*/
   obj = DBMakeObject(name, DB_PHZONELIST, 15);

   DBAddIntComponent(obj, "nfaces", nfaces);
   DBAddIntComponent(obj, "lnodelist", lnodelist);
   DBAddIntComponent(obj, "nzones", nzones);
   DBAddIntComponent(obj, "lfacelist", lfacelist);
   DBAddIntComponent(obj, "origin", origin);
   DBAddIntComponent(obj, "lo_offset", lo_offset);
   DBAddIntComponent(obj, "hi_offset", hi_offset);

   count[0] = nfaces;

   DBWriteComponent(dbfile, obj, "nodecnt", name, "integer",
                    nodecnt, 1, count);

   count[0] = lnodelist;

   DBWriteComponent(dbfile, obj, "nodelist", name, "integer",
                    nodelist, 1, count);

   count[0] = nzones;

   DBWriteComponent(dbfile, obj, "facecnt", name, "integer",
                    facecnt, 1, count);

   count[0] = lfacelist;

   DBWriteComponent(dbfile, obj, "facelist", name, "integer",
                    facelist, 1, count);

   if (extface)
   {
       count[0] = nfaces;

       DBWriteComponent(dbfile, obj, "extface", name, "char",
                        extface, 1, count);
   }

   if (_phzl._gzoneno)
   {
       count[0] = nzones;
       DBWriteComponent(dbfile, obj, "gzoneno", name, "integer",
                        _phzl._gzoneno, 1, count);
   }

   /*-------------------------------------------------------------
    *  Write object to output file.
    *-------------------------------------------------------------*/
   DBWriteObject(dbfile, obj, TRUE);
   DBFreeObject(obj);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                                  db_InitCsg
 *
 *  Purpose
 *
 *      Initialize the csg output package. This involves
 *      setting global data (vars, dim-ids, var-ids), writing standard
 *      dimensions and variables to the output file, and processing the
 *      option list provided by the caller.
 *
 *  Programmer
 *
 *      Mark C. Miller, Wed Aug  3 14:39:03 PDT 2005
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE int
db_InitCsg(DBfile *_dbfile, char *meshname, DBoptlist *optlist) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   long           count[3];
   float          a[3];
   char           tmp[256];

   db_mkname(dbfile->pdb, meshname, "typeflags", tmp);
   if (lite_PD_inquire_entry(dbfile->pdb, tmp, FALSE, NULL) != NULL) {
      db_ResetGlobalData_Csgmesh();
      db_ProcessOptlist(DB_CSGMESH, optlist);
      db_build_shared_names_csgmesh(_dbfile, meshname);
      return 0;
   }

   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/
   db_ResetGlobalData_Csgmesh();
   db_ProcessOptlist(DB_CSGMESH, optlist);
   db_build_shared_names_csgmesh(_dbfile, meshname);

   /*  Write some scalars */
   count[0] = 1L;
   if (_csgm._time_set == TRUE)
      PJ_write_len(dbfile->pdb, _csgm._nm_time, "float", &(_csgm._time), 1, count);
   if (_csgm._dtime_set == TRUE)
      PJ_write_len(dbfile->pdb, _csgm._nm_dtime, "double",
                   &(_csgm._dtime), 1, count);

   PJ_write_len(dbfile->pdb, _csgm._nm_cycle, "integer", &(_csgm._cycle),
                1, count);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                                  db_InitPoint
 *
 *  Purpose
 *
 *      Initialize the point output package. This involves
 *      setting global data (vars, dim-ids, var-ids), writing standard
 *      dimensions and variables to the output file, and processing the
 *      option list provided by the caller.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *      Al Leibee, Mon Apr 18 07:45:58 PDT 1994
 *      Added _dtime.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE int
db_InitPoint (DBfile *_dbfile, DBoptlist *optlist, int ndims, int nels) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   long           count[3];

   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/
   db_ResetGlobalData_PointMesh(ndims);
   db_ProcessOptlist(DB_POINTMESH, optlist);

   /*--------------------------------------------------
    *  Assign values to global data.
    *--------------------------------------------------*/
   _pm._nels = nels;

   _pm._minindex = _pm._lo_offset;
   _pm._maxindex = nels - _pm._hi_offset - 1;

   _pm._coordnames[0] = "xpt_data";
   _pm._coordnames[1] = "ypt_data";
   _pm._coordnames[2] = "zpt_data";

   /*------------------------------------------------------
    * Define all attribute arrays and scalars which are
    * likely to be needed. Some require a zonal AND nodal
    * version.
    *-----------------------------------------------------*/

   /*  Write some scalars */
   count[0] = 1L;
   if (_pm._time_set == TRUE) {
      db_mkname(dbfile->pdb, NULL, "time", _pm._nm_time);
      PJ_write_len(dbfile->pdb, _pm._nm_time, "float", &(_pm._time),
                   1, count);
   }

   if (_pm._dtime_set == TRUE) {
      db_mkname(dbfile->pdb, NULL, "dtime", _pm._nm_dtime);
      PJ_write_len(dbfile->pdb, _pm._nm_dtime, "double", &(_pm._dtime),
                   1, count);
   }

   db_mkname(dbfile->pdb, NULL, "cycle", _pm._nm_cycle);
   PJ_write_len(dbfile->pdb, _pm._nm_cycle, "integer", &(_pm._cycle),
                1, count);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                                  db_InitQuad
 *
 *  Purpose
 *
 *      Initialize the quadrilateral output package. This involves
 *      setting global data (vars, dim-ids, var-ids), writing standard
 *      dimensions and variables to the SILO file, and processing the
 *      option list provided by the caller.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *  Modifications
 *
 *     Al Leibee, Sun Apr 17 07:54:25 PDT 1994
 *     Added dtime.
 *      Al Leibee, Mon Aug  9 10:30:54 PDT 1993
 *      Converted to new PD_inquire_entry interface.
 *
 *     Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *     Added baseindex[].
 *
 *     Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *     Removed unused variable error.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE int
db_InitQuad (DBfile *_dbfile, char *meshname, DBoptlist *optlist,
             int dims[], int ndims) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   int            i;
   int            nzones, nnodes;
   long           count[3];
   float          a[3];
   char           tmp[1024];
   char          *p=NULL;

   /*--------------------------------------------------
    *  Define number of zones and nodes.
    *--------------------------------------------------*/
   nzones = nnodes = 1;

   for (i = 0; i < ndims; i++) {
      nzones *= (dims[i] - 1);
      nnodes *= dims[i];
   }

   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/

   db_ResetGlobalData_QuadMesh(ndims);
   db_ProcessOptlist(DB_QUADMESH, optlist);
   db_build_shared_names_quadmesh(_dbfile, meshname);

   /*--------------------------------------------------
    *  Determine if this directory has already been
    *  initialized for the given mesh. If so, set
    *  the names of the shared variables (e.g., 'dims')
    *  and return. If not, write out the dimensions
    *  and set the names.
    *--------------------------------------------------*/

   db_mkname(dbfile->pdb, meshname, "dims", tmp);

   if (lite_PD_inquire_entry(dbfile->pdb, tmp, FALSE, NULL) != NULL) {
      return 0;
   }

   /*--------------------------------------------------
    *  Assign values to global data.
    *--------------------------------------------------*/
   _qm._nzones = nzones;
   _qm._nnodes = nnodes;
   _qm._meshname = STRDUP(meshname);

   for (i = 0; i < ndims; i++) {
      _qm._dims[i] = dims[i];
      _qm._zones[i] = dims[i] - 1;

      _qm._minindex[i] = _qm._lo_offset[i];
      _qm._maxindex_n[i] = dims[i] - _qm._hi_offset[i] - 1;
      _qm._maxindex_z[i] = _qm._maxindex_n[i] - 1;
   }

   /*--------------------------------------------------
    * This directory is uninitialized.
    *
    * Define all attribute arrays and scalars which are
    * likely to be needed. Some require a zonal AND nodal
    * version.
    *-----------------------------------------------------*/

   count[0] = ndims;
   /*  File name contained within meshname */
   p = strchr(meshname, ':');
   if (p == NULL) {
      PJ_write_len(dbfile->pdb, _qm._nm_dims, "integer", dims, 1, count);
      PJ_write_len(dbfile->pdb, _qm._nm_zones, "integer", _qm._zones,
                1, count);
      PJ_write_len(dbfile->pdb, _qm._nm_maxindex_n, "integer",
                _qm._maxindex_n, 1, count);
      PJ_write_len(dbfile->pdb, _qm._nm_maxindex_z, "integer",
                _qm._maxindex_z, 1, count);
      PJ_write_len(dbfile->pdb, _qm._nm_minindex, "integer",
                _qm._minindex, 1, count);
      PJ_write_len(dbfile->pdb, _qm._nm_baseindex, "integer",
                _qm._baseindex, 1, count);

      a[0] = a[1] = a[2] = 0.5;
      PJ_write_len(dbfile->pdb, _qm._nm_alignz, "float", a, 1, count);

      a[0] = a[1] = a[2] = 0.0;
      PJ_write_len(dbfile->pdb, _qm._nm_alignn, "float", a, 1, count);
   }

   /*  Write some scalars */
   count[0] = 1L;

   if (_qm._time_set == TRUE) {
      PJ_write_len(dbfile->pdb, _qm._nm_time, "float", &(_qm._time), 1, count);
   }
   if (_qm._dtime_set == TRUE) {
      PJ_write_len(dbfile->pdb, _qm._nm_dtime, "double",
                   &(_qm._dtime), 1, count);
   }
   PJ_write_len(dbfile->pdb, _qm._nm_cycle, "integer",
                &(_qm._cycle), 1, count);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                                  db_InitUcd
 *
 *  Purpose
 *
 *      Initialize the uc output package. This involves
 *      setting global data (vars, dim-ids, var-ids), writing standard
 *      dimensions and variables to the output file, and processing the
 *      option list provided by the caller.
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Notes
 *
 *  Modifications
 *
 *      Al Leibee, Mon Aug  9 10:30:54 PDT 1993
 *      Converted to new PD_inquire_entry interface.
 *
 *      Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *      Removed unused variable error.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE int
db_InitUcd (DBfile *_dbfile, char *meshname, DBoptlist *optlist,
            int ndims, int nnodes, int nzones) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   long           count[3];
   float          a[3];
   char           tmp[256];
   char          *p=NULL;

   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/
   db_ResetGlobalData_Ucdmesh(ndims, nnodes, nzones);
   db_ProcessOptlist(DB_UCDMESH, optlist);
   db_build_shared_names_ucdmesh(_dbfile, meshname);

   /*--------------------------------------------------
    *  Determine if this directory has already been
    *  initialized. Return if it has.
    *--------------------------------------------------*/

   db_mkname(dbfile->pdb, meshname, "align_zonal", tmp);

   if (lite_PD_inquire_entry(dbfile->pdb, tmp, FALSE, NULL) != NULL) {
      return 0;
   }

   /*--------------------------------------------------
    *  Assign values to global data.
    *--------------------------------------------------*/
   _um._nzones = nzones;
   _um._nnodes = nnodes;

   /*------------------------------------------------------
    * Define all attribute arrays and scalars which are
    * likely to be needed. Some require a zonal AND nodal
    * version.
    *-----------------------------------------------------*/
   count[0] = ndims;

   /*------------------------------------------------------
    * Assume that if there is a file in the meshname, that 
    * there is possibily another  mesh that may be pointing to,
    * so we'll use that meshes discriptions.
    *-----------------------------------------------------*/
   p = strchr(meshname, ':');
   if (p == NULL) {
      a[0] = a[1] = a[2] = 0.5;
      PJ_write_len(dbfile->pdb, _um._nm_alignz, "float", a, 1, count);
   
      a[0] = a[1] = a[2] = 0.0;
      PJ_write_len(dbfile->pdb, _um._nm_alignn, "float", a, 1, count);
   }

   /*  Write some scalars */
   count[0] = 1L;
   if (_um._time_set == TRUE)
      PJ_write_len(dbfile->pdb, _um._nm_time, "float", &(_um._time), 1, count);
   if (_um._dtime_set == TRUE)
      PJ_write_len(dbfile->pdb, _um._nm_dtime, "double",
                   &(_um._dtime), 1, count);

   PJ_write_len(dbfile->pdb, _um._nm_cycle, "integer", &(_um._cycle),
                1, count);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                              db_InitZonelist
 *
 *  Purpose
 *
 *      Initialize the ucd zonelist output package.  This involves
 *      setting global data and processing the option list provided
 *      by the caller.
 *
 *  Programmer
 *
 *      Jeremy Meredith, May 21 1999
 *
 *  Notes
 *
 *  Modifications
 *
 *     Hank Childs, Thu Jan  6 13:51:22 PST 2000
 *     Put in lint directive for unused arguments. Removed unused
 *     argument dbfile.
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
/* ARGSUSED */
PRIVATE int
db_InitZonelist (DBfile *_dbfile, DBoptlist *optlist)
{
   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/
   db_ResetGlobalData_Ucdzonelist();
   db_ProcessOptlist(DB_ZONELIST, optlist);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                          db_InitPHZonelist
 *
 *  Purpose
 *
 *      Initialize the polyhedral zonelist output package.  This involves
 *      setting global data and processing the option list provided
 *      by the caller.
 *
 *  Programmer
 *
 *      Mark C. Miller, July 27, 2004
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
/* ARGSUSED */
PRIVATE int
db_InitPHZonelist (DBfile *_dbfile, DBoptlist *optlist)
{
   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/
   db_ResetGlobalData_phzonelist();
   db_ProcessOptlist(DB_PHZONELIST, optlist);

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                db_build_shared_names_csgmesh
 *
 *  Purpose
 *
 *      Build the names of the shared variables, based on the name
 *      of the current mesh. Store the names in the global variables
 *      (which all begin with the prefix "_nm_".
 *
 *  Programmer
 *
 *      Mark C. Miller, Wed Aug  3 14:39:03 PDT 2005 
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE void
db_build_shared_names_csgmesh(DBfile *_dbfile, char *meshname) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;

   if (_csgm._time_set == TRUE)
      db_mkname(dbfile->pdb, NULL, "time", _csgm._nm_time);
   if (_csgm._dtime_set == TRUE)
      db_mkname(dbfile->pdb, NULL, "dtime", _csgm._nm_dtime);

   db_mkname(dbfile->pdb, NULL, "cycle", _csgm._nm_cycle);
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                  db_build_shared_names_quadmesh
 *
 *  Purpose
 *
 *      Build the names of the shared variables, based on the name
 *      of the current mesh. Store the names in the global variables
 *      (which all begin with the prefix "_nm_").
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Modifications
 *
 *     Al Leibee, Sun Apr 17 07:54:25 PDT 1994
 *     Added dtime.
 *
 *     Jeremy Meredith, Fri May 21 10:04:25 PDT 1999
 *     Added _nm_baseindex.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE void
db_build_shared_names_quadmesh (DBfile *_dbfile, char *meshname) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;

   db_mkname(dbfile->pdb, meshname, "dims", _qm._nm_dims);
   db_mkname(dbfile->pdb, meshname, "zonedims", _qm._nm_zones);
   db_mkname(dbfile->pdb, meshname, "max_index_n", _qm._nm_maxindex_n);
   db_mkname(dbfile->pdb, meshname, "max_index_z", _qm._nm_maxindex_z);
   db_mkname(dbfile->pdb, meshname, "min_index", _qm._nm_minindex);
   db_mkname(dbfile->pdb, meshname, "align_zonal", _qm._nm_alignz);
   db_mkname(dbfile->pdb, meshname, "align_nodal", _qm._nm_alignn);
   db_mkname(dbfile->pdb, meshname, "baseindex", _qm._nm_baseindex);

   if (_qm._time_set == TRUE)
      db_mkname(dbfile->pdb, NULL, "time", _qm._nm_time);
   if (_qm._dtime_set == TRUE)
      db_mkname(dbfile->pdb, NULL, "dtime", _qm._nm_dtime);

   db_mkname(dbfile->pdb, NULL, "cycle", _qm._nm_cycle);
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                              db_ResetGlobalData_phzonelist
 *
 *  Purpose
 *
 *      Reset global data to default values. For internal use only.
 *
 *  Programmer
 *
 *      Mark C. Miller, July 27, 2004 
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE int
db_ResetGlobalData_phzonelist (void) {

   memset(&_phzl, 0, sizeof(_phzl));

   return 0;
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                db_build_shared_names_ucdmesh
 *
 *  Purpose
 *
 *      Build the names of the shared variables, based on the name
 *      of the current mesh. Store the names in the global variables
 *      (which all begin with the prefix "_nm_".
 *
 *  Programmer
 *
 *      Jeffery W. Long, NSSD-B
 *
 *  Modifications
 *
 *     Al Leibee, Mon Apr 18 07:45:58 PDT 1994
 *     Added _dtime.
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE void
db_build_shared_names_ucdmesh (DBfile *_dbfile, char *meshname) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;

   db_mkname(dbfile->pdb, meshname, "align_zonal", _um._nm_alignz);
   db_mkname(dbfile->pdb, meshname, "align_nodal", _um._nm_alignn);
   if (_um._time_set == TRUE)
      db_mkname(dbfile->pdb, NULL, "time", _um._nm_time);
   if (_um._dtime_set == TRUE)
      db_mkname(dbfile->pdb, NULL, "dtime", _um._nm_dtime);

   db_mkname(dbfile->pdb, NULL, "cycle", _um._nm_cycle);
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 *  Routine                                                    db_mkname
 *
 *  Purpose
 *
 *      Generate a name based on current directory, given name, and
 *      the given suffix.
 *
 *  Modifications:
 *      Sean Ahern, Mon Dec 18 17:48:30 PST 2000
 *      If the "name" starts with "/", then we don't need to determine our
 *      current working directory.  I added this logic path.
 *
 *      Eric Brugger, Fri Apr  6 11:15:37 PDT 2001
 *      I changed a comparison between an integer and NULL to an integer
 *      and 0.
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE void
db_mkname (PDBfile *pdb, char *name, char *suffix, char *out)
{
    char   *cwd;

    out[0] = '\0';

    /* Check if the "name" starts with "/", indicating an absolute
     * pathname. */
    if (!name || (strncmp(name, "/", 1) != 0))
    {
        /* Get the directory name. */
        if ((cwd = lite_PD_pwd(pdb)))
        {
            strcat(out, cwd);
            if (!STR_EQUAL("/", cwd))
                strcat(out, "/");
        } else
        {
            strcat(out, "/");
        }
    }

    /* And tack on the file name and extension is supplied. */
    if (name)
        strcat(out, name);
    if (suffix)
    {
        if (name)
            strcat(out, "_");
        strcat(out, suffix);
    }
}
#endif /* PDB_WRITE */

/*----------------------------------------------------------------------
 * Routine                                                  db_InitMulti
 *
 * Purpose
 *
 *    Initialize the quadrilateral output package. This involves
 *    setting global data (vars, dim-ids, var-ids), writing standard
 *    dimensions and variables to the SILO file, and processing the
 *    option list provided by the caller.
 *
 * Programmer
 *
 *    Eric Brugger, January 12, 1996
 *
 * Notes
 *
 * Modifications
 *
 *--------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE int
db_InitMulti (DBfile *_dbfile, DBoptlist *optlist) {

   DBfile_pdb    *dbfile = (DBfile_pdb *) _dbfile;
   long           count[3];

   /*--------------------------------------------------
    *  Process the given option list (this function
    *  modifies the global variable data based on
    *  the option values.)
    *--------------------------------------------------*/
   db_ResetGlobalData_MultiMesh();
   db_ProcessOptlist(DB_MULTIMESH, optlist);

   /*------------------------------------------------------
    * Define all attribute arrays and scalars which are
    * likely to be needed. Some require a zonal AND nodal
    * version.
    *-----------------------------------------------------*/

   /*  Write some scalars */
   count[0] = 1L;
   if (_mm._time_set == TRUE) {
      db_mkname(dbfile->pdb, NULL, "time", _mm._nm_time);
      PJ_write_len(dbfile->pdb, _mm._nm_time, "float", &(_mm._time),
                   1, count);
   }

   if (_mm._dtime_set == TRUE) {
      db_mkname(dbfile->pdb, NULL, "dtime", _mm._nm_dtime);
      PJ_write_len(dbfile->pdb, _mm._nm_dtime, "double", &(_mm._dtime),
                   1, count);
   }

   db_mkname(dbfile->pdb, NULL, "cycle", _mm._nm_cycle);
   PJ_write_len(dbfile->pdb, _mm._nm_cycle, "integer", &(_mm._cycle),
                1, count);

   return 0;
}
#endif /* PDB_WRITE */


/*-------------------------------------------------------------------------
 * Function:    db_InitCurve
 *
 * Purpose:     Inititalize global curve data.
 *
 * Return:      void
 *
 * Programmer:  Robb Matzke
 *              robb@callisto.nuance.com
 *              May 16, 1996
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE void
db_InitCurve (DBoptlist *opts) {

   db_ResetGlobalData_Curve () ;
   db_ProcessOptlist (DB_CURVE, opts) ;
}
#endif /* PDB_WRITE */

/*-------------------------------------------------------------------------
 * Function:    db_InitDefvars
 *
 * Purpose:     Inititalize global defvars data.
 *
 * Return:      void
 *
 * Programmer:  Mark C. Miller 
 *              March 22, 2006
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------*/
#ifdef PDB_WRITE
PRIVATE void
db_InitDefvars (DBoptlist *opts) {

   db_ResetGlobalData_Defvars () ;
   db_ProcessOptlist (DB_DEFVARS, opts) ;
}
#endif /* PDB_WRITE */