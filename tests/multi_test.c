/*---------------------------------------------------------------------------
 * multi_testall.c -- Multi-Block Test File Generator.
 *
 * Programmed by Katherine Price, August 4, 1995
 *
 * This test file creates multi-block objects, that are based on the same
 * data sets as the objects created by testall.c.
 *
 *	multi_rect2d.*  - 12 blocks	(3 x 4)
 *	multi_curv2d.*  -  5 blocks	(5 x 1)
 *	multi_point2d.* -  5 blocks	(5 x 1)
 *	multi_rect3d.*  - 36 blocks	(3 x 4 x 3)
 *	multi_curv3d.*  - 36 blocks	(3 x 4 x 3)
 *	multi_ucd3d.*   - 20 blocks	(1 x 1 x 20)
 *
 * Note: The multi_curv3d.* file has not been tested.
 *
 * Modifications:
 *     Sean Ahern, Thu Jun 13 11:52:47 PDT 1996
 *     Got rid of the ^Ls in the file.
 *
 *     Sean Ahern, Tue Dec  7 16:07:04 PST 1999
 *     Changed functions to ANSI style.  Reformatted code through indent.
 *
 *     Lisa J. Roberts, Fri Apr  7 10:30:59 PDT 2000
 *     Added string.h to get rid of compiler warnings.
 *
 *-------------------------------------------------------------------------*/

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <silo.h>
#include <swat.h>

#define MAXBLOCKS       100           /* maximum number of blocks in an object   */
#define MAXNUMVARS      10            /* maximum number of vars to output */
#define STRLEN          60
#define MIXMAX          20000       /* Maximum length of the mixed arrays */
#define MAXMATNO        3

#define SET_OPTIONS(ES,EX,ZCNTS,MLEN,MCNTS,MLISTS,HASEXT)   \
    if (optlist) DBFreeOptlist(optlist);                    \
    optlist = DBMakeOptlist(20);                            \
    DBAddOption(optlist, DBOPT_CYCLE, &cycle);              \
    DBAddOption(optlist, DBOPT_TIME, &time);                \
    DBAddOption(optlist, DBOPT_DTIME, &dtime);              \
    DBAddOption(optlist, DBOPT_NMATNOS, &nmatnos);          \
    DBAddOption(optlist, DBOPT_MATNOS, matnos);             \
    DBAddOption(optlist, DBOPT_EXTENTS_SIZE, &ES);          \
    if (EX)                                                 \
       DBAddOption(optlist, DBOPT_EXTENTS, EX);             \
    if (ZCNTS)                                              \
       DBAddOption(optlist, DBOPT_ZONECOUNTS, ZCNTS);       \
    if (MLEN)                                               \
       DBAddOption(optlist, DBOPT_MIXLENS, MLEN);           \
    if (MCNTS)                                              \
       DBAddOption(optlist, DBOPT_MATCOUNTS, MCNTS);        \
    if (MLISTS)                                             \
       DBAddOption(optlist, DBOPT_MATLISTS, MLISTS);        \
    if (HASEXT)                                             \
       DBAddOption(optlist, DBOPT_HAS_EXTERNAL_ZONES, HASEXT)


double varextents[MAXNUMVARS][2*MAXBLOCKS];
int mixlens[MAXBLOCKS];
int zonecounts[MAXBLOCKS];
int has_external_zones[MAXBLOCKS];
int matcounts[MAXBLOCKS];
int matlists[MAXBLOCKS][MAXMATNO+1];
int driver = DB_PDB;

int           build_multi(DBfile *, int, int, int, int, int, int, int);

void          build_block_rect2d(DBfile *, char[MAXBLOCKS][STRLEN], int, int);
void          build_block_curv2d(DBfile *, char[MAXBLOCKS][STRLEN], int, int);
void          build_block_point2d(DBfile *, char[MAXBLOCKS][STRLEN], int, int);
void          build_block_rect3d(DBfile *, char[MAXBLOCKS][STRLEN], int, int,
                                 int);
void          build_block_curv3d(DBfile *, char[MAXBLOCKS][STRLEN], int, int,
                                 int);
void          build_block_ucd3d(DBfile *, char[MAXBLOCKS][STRLEN], int, int,
                                int);

static void   put_extents(float *arr, int len, double *ext_arr, int block);
static void   fill_rect3d_bkgr(int matlist[], int nx, int ny, int nz,
                               int matno);
static void   fill_rect3d_mat(float x[], float y[], float z[], int matlist[],
                              int nx, int ny, int nz, int mix_next[],
                              int mix_mat[], int mix_zone[], float mix_vf[],
                              int *mixlen, int matno, double xcenter,
                              double ycenter, double zcenter, double radius);

/*-------------------------------------------------------------------------
 * Function:    put_extents
 *
 * Purpose:     Compute the extents of the given data and put in in the
 *              specified array for the specified block
 *
 * Programmer:  Mark C. Miller, 07Aug03
 *
 * Modifications:
 *
 *------------------------------------------------------------------------*/
static void
put_extents(float *arr, int len, double *ext_arr, int block)
{
   int i;
   double min = arr[0], max = min;
   for (i = 0; i < len; i++)
   {
      if (arr[i] < min)
         min = arr[i];
      if (arr[i] > max)
         max = arr[i];
   }
   ext_arr[2*block] = min;
   ext_arr[2*block+1] = max;
}

/*-------------------------------------------------------------------------
 * Function:    count_mats 
 *
 * Purpose:     Count the number of unique materials in a block
 *              To count the number of unique materials actaully in this
 *              block, we use an approximate algorithm where we assume every
 *              block that contains a given material, does not contain it
 *              only in a mixed element. In this way, we can count materials
 *              be examining only the clean material data
 *
 * Programmer:  Mark C. Miller, 14Aug03
 *
 * Modifications:
 *
 *------------------------------------------------------------------------*/
static int 
count_mats(int nzones, int *matlist, int *unique_mats)
{
   int i, num_mats = 0;
   int mat_map[MAXMATNO+1] = {0,0,0,0};
   for (i = 0; i < nzones; i++)
   {
      if (matlist[i]>=0)
         mat_map[matlist[i]] = 1;
   }
   for (i = 0; i < MAXMATNO+1; i++)
      if (mat_map[i] == 1)
         unique_mats[num_mats++] = i;

   return num_mats;
}

/*-------------------------------------------------------------------------
 * Function:    fill_rect3d_bkgr
 *
 * Purpose:     Fill the entire material array with the material "matno".
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:  Eric Brugger, 10/17/97
 *
 * Modifications:
 *
 *------------------------------------------------------------------------*/
static void
fill_rect3d_bkgr(int matlist[], int nx, int ny, int nz, int matno)
{
    int             i, j, k;

    for (i = 0; i < nx; i++)
    {
        for (j = 0; j < ny; j++)
        {
            for (k = 0; k < nz; k++)
            {
                matlist[k * nx * ny + j * nx + i] = matno;
            }
        }
    }
}

/*-------------------------------------------------------------------------
 * Function:    fill_rect3d_mat
 *
 * Purpose:     Fill the specified material array with sphere centered
 *              at "xcenter", "ycenter", "zcenter" and radius of "radius"
 *              with the material "matno".
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:  Eric Brugger, 10/17/97
 *
 * Modifications:
 *     Sean Ahern, Thu Jul  2 17:02:18 PDT 1998
 *     Fixed an indexing problem.
 *
 *------------------------------------------------------------------------*/
static void
fill_rect3d_mat(float x[], float y[], float z[], int matlist[], int nx,
                int ny, int nz, int mix_next[], int mix_mat[], int mix_zone[],
                float mix_vf[], int *mixlen, int matno, double xcenter,
                double ycenter, double zcenter, double radius)
{
    int             i, j, k, l, m, n;
    double          dist;
    int             cnt;
    int             mixlen2;
    int            *itemp;
    float           dx, dy, dz;
    float           xx[10], yy[10], zz[10];

    mixlen2 = *mixlen;

    itemp = ALLOC_N(int, (nx + 1) * (ny + 1) * (nz + 1));

    for (i = 0; i < nx+1; i++)
    {
        for (j = 0; j < ny+1; j++)
        {
            for (k = 0; k < nz+1; k++)
            {
                dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                            (y[j] - ycenter) * (y[j] - ycenter) +
                            (z[k] - zcenter) * (z[k] - zcenter));
                itemp[k * (nx + 1) * (ny + 1) + j * (nx + 1) + i] =
                    (dist < radius) ? 1 : 0;
            }
        }
    }
    for (i = 0; i < nx; i++)
    {
        for (j = 0; j < ny; j++)
        {
            for (k = 0; k < nz; k++)
            {
                cnt = itemp[(k + 0) * (nx + 1) * (ny + 1) + (j + 0) * (nx + 1) + i + 0] +
                      itemp[(k + 0) * (nx + 1) * (ny + 1) + (j + 1) * (nx + 1) + i + 0] +
                      itemp[(k + 0) * (nx + 1) * (ny + 1) + (j + 1) * (nx + 1) + i + 1] +
                      itemp[(k + 0) * (nx + 1) * (ny + 1) + (j + 0) * (nx + 1) + i + 1] +
                      itemp[(k + 1) * (nx + 1) * (ny + 1) + (j + 0) * (nx + 1) + i + 0] +
                      itemp[(k + 1) * (nx + 1) * (ny + 1) + (j + 1) * (nx + 1) + i + 0] +
                      itemp[(k + 1) * (nx + 1) * (ny + 1) + (j + 1) * (nx + 1) + i + 1] +
                      itemp[(k + 1) * (nx + 1) * (ny + 1) + (j + 0) * (nx + 1) + i + 1];
                if (cnt == 0)
                {
                    /* EMPTY */
                } else if (cnt == 8)
                {
                    matlist[k * nx * ny + j * nx + i] = matno;
                } else
                {
                    dx = (x[i + 1] - x[i]) / 11.;
                    dy = (y[j + 1] - y[j]) / 11.;
                    dz = (z[k + 1] - z[k]) / 11.;
                    for (l = 0; l < 10; l++)
                    {
                        xx[l] = x[i] + (dx / 2.) + (l * dx);
                        yy[l] = y[j] + (dy / 2.) + (l * dy);
                        zz[l] = z[k] + (dz / 2.) + (l * dz);
                    }
                    cnt = 0;
                    for (l = 0; l < 10; l++)
                    {
                        for (m = 0; m < 10; m++)
                        {
                            for (n = 0; n < 10; n++)
                            {
                                dist = sqrt((xx[l] - xcenter) *
                                            (xx[l] - xcenter) +
                                            (yy[m] - ycenter) *
                                            (yy[m] - ycenter) +
                                            (zz[n] - zcenter) *
                                            (zz[n] - zcenter));
                                cnt += (dist < radius) ? 1 : 0;
                            }
                        }
                    }
                    matlist[k * nx * ny + j * nx + i] = -(mixlen2 + 1);
                    mix_mat[mixlen2] = matno - 1;
                    mix_mat[mixlen2 + 1] = matno;
                    mix_next[mixlen2] = mixlen2 + 2;
                    mix_next[mixlen2 + 1] = 0;
                    mix_zone[mixlen2] = k * nx * ny + j * nx + i;
                    mix_zone[mixlen2 + 1] = k * nx * ny + j * nx + i;
                    mix_vf[mixlen2] = 1. - (((float)cnt) / 1000.);
                    mix_vf[mixlen2 + 1] = ((float)cnt) / 1000.;
                    mixlen2 += 2;
                }
            }
        }
    }

    FREE(itemp);

    *mixlen = mixlen2;
}

/*-------------------------------------------------------------------------
 * Function:    main
 *
 * Purpose:     Generate multi block test files.
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:  Katherine Price, 8/4/95
 *
 * Modifications:
 *    Eric Brugger, Fri Oct 17 17:05:32 PDT 1997
 *    Modified the number of blocks in the ucd3d sample file.
 *
 *    Eric Brugger, Mon Mar 2  20:46:27 PDT 1998
 *    Added the creation of a 2d multi-block point mesh.
 *
 *    Sean Ahern, Thu Jul  2 10:57:15 PDT 1998
 *    Added a return statement.
 *
 *    Robb Matzke, 1999-04-09
 *    Added argument parsing to control the driver which is used.
 *
 *    Sean Ahern, Tue Dec  7 16:07:31 PST 1999
 *    Fixed the type of argv.
 *
 *------------------------------------------------------------------------*/
int
main(int argc, char *argv[])
{
    DBfile         *dbfile;
    char           filename[256], *file_ext=".pdb";
    int            i;
    int            dochecks = FALSE;

    /* Parse command-line */
    for (i=1; i<argc; i++) {
        if (!strcmp(argv[i], "DB_PDB")) {
            driver = DB_PDB;
            file_ext = ".pdb";
        } else if (!strcmp(argv[i], "DB_HDF5")) {
            driver = DB_HDF5;
            file_ext = ".h5";
        } else if (!strcmp(argv[i], "DB_HDF5_SEC2")) {
            driver = DB_HDF5_SEC2;
            file_ext = ".h5";
        } else if (!strcmp(argv[i], "DB_HDF5_STDIO")) {
            driver = DB_HDF5_STDIO;
            file_ext = ".h5";
        } else if (!strcmp(argv[i], "DB_HDF5_CORE")) {
            int inc = 512 << 11;
            driver = inc | DB_HDF5_CORE;
            file_ext = ".h5";
        } else if (!strcmp(argv[i], "check")) {
            dochecks = TRUE;
        } else {
            fprintf(stderr, "%s: ignored argument `%s'\n", argv[0], argv[i]);
        }
    }

    DBSetEnableChecksums(dochecks);

    /*
     * Create the multi-block rectilinear 2d mesh.
     */
    sprintf(filename, "multi_rect2d%s", file_ext);
    fprintf(stdout, "creating %s\n", filename);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                         "multi-block rectilinear 2d test file", driver))
        == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
    } else if (build_multi(dbfile, DB_QUADMESH, DB_QUADVAR, 2, 3, 4, 1,
                           DB_COLLINEAR) == -1)
    {
        fprintf(stderr, "Error in creating '%s'.\n", filename);
        DBClose(dbfile);
    } else
        DBClose(dbfile);

    /* 
     * Create the multi-block curvilinear 2d mesh.
     */
    sprintf(filename, "multi_curv2d%s", file_ext);
    fprintf(stdout, "creating %s\n", filename);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                         "multi-block curvilinear 2d test file", driver))
        == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
    } else if (build_multi(dbfile, DB_QUADMESH, DB_QUADVAR, 2, 5, 1, 1,
                           DB_NONCOLLINEAR) == -1)
    {
        fprintf(stderr, "Error in creating '%s'.\n", filename);
        DBClose(dbfile);
    } else
        DBClose(dbfile);

    /* 
     * Create the multi-block point 2d mesh.
     */
    sprintf(filename, "multi_point2d%s", file_ext);
    fprintf(stdout, "creating %s\n", filename);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                           "multi-block point 2d test file", driver))
        == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
    } else if (build_multi(dbfile, DB_POINTMESH, DB_POINTVAR, 2, 5, 1, 1,
                           0) == -1)
    {
        fprintf(stderr, "Error in creating '%s'.\n", filename);
        DBClose(dbfile);
    } else
        DBClose(dbfile);

    /* 
     * Create the multi-block rectilinear 3d mesh.
     */
    sprintf(filename, "multi_rect3d%s", file_ext);
    fprintf(stdout, "creating %s\n", filename);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                         "multi-block rectilinear 3d test file", driver))
        == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
    } else if (build_multi(dbfile, DB_QUADMESH, DB_QUADVAR, 3, 3, 4, 3,
                           DB_COLLINEAR) == -1)
    {
        fprintf(stderr, "Error in creating '%s'.\n", filename);
        DBClose(dbfile);
    } else
        DBClose(dbfile);

    /* 
     * Create the multi-block curvilinear 3d mesh.
     */
    sprintf(filename, "multi_curv3d%s", file_ext);
    fprintf(stdout, "creating %s\n", filename);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                         "multi-block curvilinear 3d test file", driver))
        == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
    } else if (build_multi(dbfile, DB_QUADMESH, DB_QUADVAR, 3, 3, 4, 3,
                           DB_NONCOLLINEAR) == -1)
    {
        fprintf(stderr, "Error in creating '%s'.\n", filename);
        DBClose(dbfile);
    } else
        DBClose(dbfile);

    /* 
     * Create the multi-block ucd 3d mesh.
     */
    sprintf(filename, "multi_ucd3d%s", file_ext);
    fprintf(stdout, "creating %s\n", filename);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                           "multi-block ucd 3d test file", driver))
        == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
    } else if (build_multi(dbfile, DB_UCDMESH, DB_UCDVAR, 3, 3, 4, 3,
                           0) == -1)
    {
        fprintf(stderr, "Error in creating '%s'.\n", filename);
        DBClose(dbfile);
    } else
        DBClose(dbfile);

    return (0);
}                                      /* main */

/*-------------------------------------------------------------------------
 * Function:    build_multi
 *
 * Purpose:     Make a multi-block mesh, multi-block variables, and a
 *              multi-block material based on the given meshtype,
 *              dimensionality of the mesh, and number of blocks in the
 *              x-direction, y-direction, and z-direction.  Also specify
 *              if the mesh is collinear when creating a quad mesh.  The
 *              total number of blocks created for the mesh equals
 *              number of blocks in x-direction times number of blocks in
 *              y-direction times number of blocks in z-direction.
 *
 *              nblocks = nblocks_x * nblocks_y * nblocks_z
 *
 * Return:      Success: 0
 *
 *              Failure: -1
 *
 * Programmer:  Katherine Price, 5/19/95
 *
 * Modifications:
 *    Eric Brugger, Tue Jan 16 08:53:59 PST 1996
 *    I modified the multi-block calls to output time, dtime, and cycle
 *    in the option lists.
 *
 *    Eric Brugger, Fri Oct 17 17:09:00 PDT 1997
 *    I modified the routine to output the materials and number of materials
 *    associated with the multimat.
 *
 *    Eric Brugger, Mon Mar 2  20:46:27 PDT 1998
 *    Added the creation of a 2d multi-block point mesh.
 *
 *    Sean Ahern, Thu Jul  2 11:03:06 PDT 1998
 *    Fixed a memory leak where we didn't free the optlist.
 *
 *    Jeremy Meredith, Tue Oct  4 12:28:35 PDT 2005
 *    Renamed defvar types to avoid namespace collision.
 *
 *    Mark C. Miller, Mon Aug  7 17:03:51 PDT 2006
 *    Added additional material object with material names and colors 
 *------------------------------------------------------------------------*/
int
build_multi(DBfile *dbfile, int meshtype, int vartype, int dim, int nblocks_x,
            int nblocks_y, int nblocks_z, int coord_type)
{
    int             i,j,k;
    int             cycle;
    float           time;
    double          dtime;
    int             nmatnos;
    int             matnos[3];
    char            names[MAXBLOCKS][STRLEN];
    char           *meshnames[MAXBLOCKS];
    int             meshtypes[MAXBLOCKS];
    char            names1[MAXBLOCKS][STRLEN];
    char            names2[MAXBLOCKS][STRLEN];
    char            names3[MAXBLOCKS][STRLEN];
    char            names4[MAXBLOCKS][STRLEN];
    char            names5[MAXBLOCKS][STRLEN];
    char           *var1names[MAXBLOCKS];
    char           *var2names[MAXBLOCKS];
    char           *var3names[MAXBLOCKS];
    char           *var4names[MAXBLOCKS];
    char           *var5names[MAXBLOCKS];
    int             vartypes[MAXBLOCKS];
    char            names0[MAXBLOCKS][STRLEN];
    char           *matnames[MAXBLOCKS];
    char            dirnames[MAXBLOCKS][STRLEN];

    DBoptlist      *optlist = NULL;
    int             one = 1;

    int             nblocks = nblocks_x * nblocks_y * nblocks_z;
    int             extentssize;
    int            *tmpList;
    double         *tmpExtents;

    /* 
     * Initialize a simple grouping
     */
    int             ngroupings;
    int             groupings[9];
    char          **groupingnames = NULL;
    ngroupings = 9;            /* number of elements in the grouping arrays */
    groupings[0] = 5;          /* number of elements in this group */
    groupings[1] = 0;
    groupings[2] = 1;
    groupings[3] = 2;
    groupings[4] = 3;
    groupings[5] = 4;
    groupings[6] = 2;          /* number of elements in next group */
    groupings[7] = 5;
    groupings[8] = 6;
    groupingnames = (char**)malloc(sizeof(char*)*ngroupings);
    groupingnames[0] = safe_strdup("First Grouping");
    groupingnames[1] = safe_strdup("Zero");
    groupingnames[2] = safe_strdup("One");
    groupingnames[3] = safe_strdup("Two");
    groupingnames[4] = safe_strdup("Three");
    groupingnames[5] = safe_strdup("Four");
    groupingnames[6] = safe_strdup("Second Grouping");
    groupingnames[7] = safe_strdup("Five");
    groupingnames[8] = safe_strdup("Six");

    /* 
     * Initialize the names and create the directories for the blocks.
     */

    for (i = 0; i < nblocks; i++)
    {

        sprintf(names[i], "/block%d/mesh1", i);
        meshnames[i] = names[i];
        meshtypes[i] = meshtype;

        sprintf(names1[i], "/block%d/d", i);
        sprintf(names2[i], "/block%d/p", i);
        sprintf(names3[i], "/block%d/u", i);
        sprintf(names4[i], "/block%d/v", i);
        sprintf(names5[i], "/block%d/w", i);
        var1names[i] = names1[i];
        var2names[i] = names2[i];
        var3names[i] = names3[i];
        var4names[i] = names4[i];
        var5names[i] = names5[i];
        vartypes[i] = vartype;

        sprintf(names0[i], "/block%d/mat1", i);
        matnames[i] = names0[i];

        /* make the directory for the block mesh */

        sprintf(dirnames[i], "/block%d", i);

        if (DBMkDir(dbfile, dirnames[i]) == -1)
        {
            fprintf(stderr, "Could not make directory \"%s\"\n", dirnames[i]);
            return (-1);
        }                              /* if */
    }                                  /* for */

    /* create the blocks */

    switch (meshtype)
    {
    case DB_QUADMESH:
        if (coord_type == DB_COLLINEAR)
        {
            if (dim == 2)
                build_block_rect2d(dbfile, dirnames, nblocks_x, nblocks_y);
            else if (dim == 3)
                build_block_rect3d(dbfile, dirnames, nblocks_x, nblocks_y,
                                   nblocks_z);
        } else if (coord_type == DB_NONCOLLINEAR)
        {
            if (dim == 2)
                build_block_curv2d(dbfile, dirnames, nblocks_x, nblocks_y);
            else if (dim == 3)
                build_block_curv3d(dbfile, dirnames, nblocks_x, nblocks_y,
                                   nblocks_z);
        }
        break;

    case DB_UCDMESH:
        if (dim == 3)
            build_block_ucd3d(dbfile, dirnames, nblocks_x, nblocks_y,
                              nblocks_z);

        break;

    case DB_POINTMESH:
        if (dim == 2)
            build_block_point2d(dbfile, dirnames, nblocks_x, nblocks_y);

        break;

    default:
        fprintf(stderr, "Bad mesh type.\n");
        return (-1);
    }                                  /* switch */

    cycle = 48;
    time = 4.8;
    dtime = 4.8;
    nmatnos = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;

    /* create the multi-block mesh, reformat extents for coords */
    extentssize = 2 * dim;
    tmpExtents = (double *) malloc(nblocks * extentssize * sizeof(double));
    for (i = 0; i < nblocks; i++)
    {
       for (j = 0; j < dim; j++)
       {
          tmpExtents[i*extentssize+j] = varextents[j][2*i];
          tmpExtents[i*extentssize+j+dim] = varextents[j][2*i+1];
       }
    }
    SET_OPTIONS(extentssize,tmpExtents,zonecounts,NULL,NULL,NULL,has_external_zones);
    DBAddOption(optlist, DBOPT_GROUPINGS_SIZE, &ngroupings);
    DBAddOption(optlist, DBOPT_GROUPINGS, groupings);
    DBAddOption(optlist, DBOPT_GROUPINGNAMES, groupingnames);
    if (DBPutMultimesh(dbfile, "mesh1", nblocks,
                       meshnames, meshtypes, optlist) == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi mesh\n");
        free(tmpExtents);
        return (-1);
    }                                  /* if */
    DBClearOption(optlist, DBOPT_GROUPINGS_SIZE);
    DBClearOption(optlist, DBOPT_GROUPINGS);
    DBClearOption(optlist, DBOPT_GROUPINGNAMES);
    
    for (i = 0; i < ngroupings; i++)
        FREE(groupingnames[i]);
    FREE(groupingnames);

    /* test hidding a multimesh */
    DBAddOption(optlist, DBOPT_HIDE_FROM_GUI, &one);
    DBPutMultimesh(dbfile, "mesh1_hidden", nblocks, meshnames, meshtypes, optlist);
    DBClearOption(optlist, DBOPT_HIDE_FROM_GUI);
    free(tmpExtents);

    /* create the multi-block variables */
    extentssize = 2;
    SET_OPTIONS(extentssize,varextents[3],NULL,NULL,NULL,NULL,NULL);
    if (DBPutMultivar(dbfile, "d", nblocks, var1names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var d\n");
        return (-1);
    }                                  /* if */
    SET_OPTIONS(extentssize,varextents[4],NULL,NULL,NULL,NULL,NULL);
    if (DBPutMultivar(dbfile, "p", nblocks, var2names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var p\n");
        return (-1);
    }                                  /* if */
    SET_OPTIONS(extentssize,varextents[5],NULL,NULL,NULL,NULL,NULL);
    if (DBPutMultivar(dbfile, "u", nblocks, var3names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var u\n");
        return (-1);
    }                                  /* if */
    SET_OPTIONS(extentssize,varextents[6],NULL,NULL,NULL,NULL,NULL);
    if (DBPutMultivar(dbfile, "v", nblocks, var4names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var v\n");
        return (-1);
    }                                  /* if */
    if (dim == 3)
    {
        SET_OPTIONS(extentssize,varextents[7],NULL,NULL,NULL,NULL,NULL);
        if (DBPutMultivar(dbfile, "w", nblocks, var5names, vartypes, optlist)
            == -1)
        {
            DBFreeOptlist(optlist);
            fprintf(stderr, "Error creating multi var w\n");
            return (-1);
        }                              /* if */
    }
    /* create the multi-block material */
    k = 0;
    for (i = 0; i < nblocks; i++)
       k += matcounts[i];
    tmpList = (int *) malloc(k * sizeof(int));
    k = 0;
    for (i = 0; i < nblocks; i++)
       for (j = 0; j < matcounts[i]; j++)
          tmpList[k++] = matlists[i][j];
    extentssize = 0;
    SET_OPTIONS(extentssize,NULL,NULL,mixlens,matcounts,tmpList,NULL);
    if (meshtype != DB_POINTMESH)
    {
        if (DBPutMultimat(dbfile, "mat1", nblocks, matnames, optlist) == -1)
        {
            DBFreeOptlist(optlist);
            fprintf(stderr, "Error creating multi material\n");
            return (-1);
        }                              /* if */

        DBAddOption(optlist, DBOPT_ALLOWMAT0, &one);
        /* add material names and colors option to this one */
        if (1)
        {
            char *colors[3] = {"yellow","cyan","black"};
            char *matrnames[3] = {"outer","middle","inner"};
            DBAddOption(optlist, DBOPT_MATCOLORS, colors);
            DBAddOption(optlist, DBOPT_MATNAMES, matrnames);
            if (DBPutMultimat(dbfile, "mat2", nblocks, matnames, optlist) == -1)
            {
                DBFreeOptlist(optlist);
                fprintf(stderr, "Error creating multi material\n");
                return (-1);
            }                              /* if */
        }
    }
    free(tmpList);
    DBFreeOptlist(optlist);

    /* create a defvars object */
    {
        int ndefs = 3;
        char   vnames[3][STRLEN], *pvnames[3];
        char   defns[3][STRLEN], *pdefns[3];
        int    types[3];
        
        types[0] = DB_VARTYPE_SCALAR;
        sprintf(vnames[0], "sum");
        pvnames[0] = vnames[0];
        if (dim == 2)
            sprintf(defns[0], "u+v");
        else
            sprintf(defns[0], "u+v+w");
        pdefns[0] = defns[0];

        types[1] = DB_VARTYPE_VECTOR;
        sprintf(vnames[1], "vec");
        pvnames[1] = vnames[1];
        if (dim == 2)
            sprintf(defns[1], "{u,v}");
        else
            sprintf(defns[1], "{u,v,w}");
        pdefns[1] = defns[1];

        types[2] = DB_VARTYPE_SCALAR;
        sprintf(vnames[2], "nmats");
        pvnames[2] = vnames[2];
        sprintf(defns[2], "nmats(mat1)");
        pdefns[2] = defns[2];

        DBPutDefvars(dbfile, "defvars", ndefs, pvnames, types, pdefns, 0);
    }

    return (0);
}                                      /* build_multi */

/*-------------------------------------------------------------------------
 * Function:    build_block_rect2d
 *
 * Purpose:     Builds a rectilinear 2-d mesh and places it in the open
 *              data file.
 *
 * Return:      Success:        void
 *
 *              Failure:
 *
 * Programmer:  robb@cloud
 *              Wed Nov 23 10:13:51 EST 1994
 *
 * Modifications:
 *              Katherine Price, Aug 4, 1995
 *              Modified function to output blocks.
 *
 *-------------------------------------------------------------------------*/
void
build_block_rect2d(DBfile *dbfile, char dirnames[MAXBLOCKS][STRLEN],
                   int nblocks_x, int nblocks_y)
{
#undef NX
#define NX 30
#undef NY
#define NY 40
#undef NZ
#define NZ 30
    int             cycle;
    float           time;
    double          dtime;
    char           *coordnames[3];
    int             ndims;
    int             dims[3], zdims[3];
    float          *coords[3];
    float           x[NX + 1], y[NY + 1];

    char           *meshname, *var1name, *var2name, *var3name, *var4name, *matname;
    float           d[NX * NY], p[NX * NY], u[(NX + 1) * (NY + 1)], v[(NX + 1) * (NY + 1)];

    int             nmats;
    int             matnos[3];
    int             matlist[NX * NY];
    int             dims2[3];
    int             mixlen;
    int             mix_mat[NX * NY];
    float           mix_vf[NX * NY];

    DBoptlist      *optlist;
    char          **matnames = NULL;

    int             i, j, k, l;
    float           xave, yave;
    float           xcenter, ycenter;
    float           dist;
    float           dx, dy;
    float           xx[20], yy[20];
    int             cnt;
    int             itemp[(NX + 1) * (NY + 1)];

    int             block;
    int             delta_x, delta_y;
    int             base_x, base_y;
    int             n_x, n_y;

    float           x2[NX + 1], y2[NY + 1];
    float           d2[NX * NY], p2[NX * NY], u2[(NX + 1) * (NY + 1)], v2[(NX + 1) * (NY + 1)];
    int             matlist2[NX * NY];
    int             mixlen2;
    int             mix_next2[NX * NY], mix_mat2[NX * NY], mix_zone2[NX * NY];
    float           mix_vf2[NX * NY];

    /*
     * Create the mesh.
     */
    meshname = "mesh1";
    coordnames[0] = "xcoords";
    coordnames[1] = "ycoords";
    coords[0] = x;
    coords[1] = y;
    ndims = 2;
    dims[0] = NX + 1;
    dims[1] = NY + 1;
    for (i = 0; i < NX + 1; i++)
        x[i] = i * (1. / NX);
    for (i = 0; i < NY + 1; i++)
        y[i] = i * (1. / NX);

    /*
     * Create the density and pressure arrays.
     */
    var1name = "d";
    var2name = "p";
    xcenter = .5;
    ycenter = .5;
    zdims[0] = NX;
    zdims[1] = NY;
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NY; j++)
        {
            xave = (x[i] + x[i + 1]) / 2.;
            yave = (y[j] + y[j + 1]) / 2.;
            dist = sqrt((xave - xcenter) * (xave - xcenter) +
                        (yave - ycenter) * (yave - ycenter));
            d[j * NX + i] = dist;
            p[j * NX + i] = 1. / (dist + .0001);
        }
    }

    /*
     * Create the velocity component arrays.
     */
    var3name = "u";
    var4name = "v";
    xcenter = .5001;
    ycenter = .5001;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                        (y[j] - ycenter) * (y[j] - ycenter));
            u[j * (NX + 1) + i] = (x[i] - xcenter) / dist;
            v[j * (NX + 1) + i] = (y[j] - ycenter) / dist;
        }
    }

    /*
     * Create the material array.
     */
    matname = "mat1";
    nmats = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;
    dims2[0] = NX;
    dims2[1] = NY;
    mixlen = 0;
    matnames = (char**)malloc(sizeof(char*)*nmats);
    matnames[0] = safe_strdup("Shredded documents");
    matnames[1] = safe_strdup("Marble");
    matnames[2] = safe_strdup("Gold bullion");

    /*
     * Put in material 1.
     */
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NY; j++)
        {
            matlist[j * NX + i] = 1;
        }
    }

    /*
     * Overlay material 2.
     */
    xcenter = .5;
    ycenter = .5;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                        (y[j] - ycenter) * (y[j] - ycenter));
            itemp[j * (NX + 1) + i] = (dist < .4) ? 1 : 0;
        }
    }
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NY; j++)
        {
            cnt = itemp[(j) * (NX + 1) + (i)] + itemp[(j + 1) * (NX + 1) + (i)] +
                itemp[(j + 1) * (NX + 1) + (i + 1)] + itemp[(j) * (NX + 1) + (i + 1)];
            if (cnt == 0)
            {
                /* do nothing */
            } else if (cnt == 4)
            {
                matlist[j * NX + i] = 2;
            } else
            {
                dx = (x[i + 1] - x[i]) / 21.;
                dy = (y[j + 1] - y[j]) / 21.;
                for (k = 0; k < 20; k++)
                {
                    xx[k] = x[i] + (dx / 2.) + (k * dx);
                    yy[k] = y[j] + (dy / 2.) + (k * dy);
                }
                cnt = 0;
                for (k = 0; k < 20; k++)
                {
                    for (l = 0; l < 20; l++)
                    {
                        dist = sqrt((xx[k] - xcenter) *
                                    (xx[k] - xcenter) +
                                    (yy[l] - ycenter) *
                                    (yy[l] - ycenter));
                        cnt += (dist < .4) ? 1 : 0;
                    }
                }
                matlist[j * NX + i] = -(mixlen + 1);
                mix_mat[mixlen] = 1;
                mix_mat[mixlen + 1] = 2;
                mix_vf[mixlen] = 1. - (((float)cnt) / 400.);
                mix_vf[mixlen + 1] = ((float)cnt) / 400.;
                mixlen += 2;
            }
        }
    }

    /*
     * Overlay material 3.
     */
    xcenter = .5;
    ycenter = .5;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                        (y[j] - ycenter) * (y[j] - ycenter));
            itemp[j * (NX + 1) + i] = (dist < .2) ? 1 : 0;
        }
    }
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NX; j++)
        {
            cnt = itemp[(j) * (NX + 1) + (i)] + itemp[(j + 1) * (NX + 1) + (i)] +
                itemp[(j + 1) * (NX + 1) + (i + 1)] + itemp[(j) * (NX + 1) + (i + 1)];
            if (cnt == 0)
            {
                /* do nothing */
            } else if (cnt == 4)
            {
                matlist[j * NX + i] = 3;
            } else
            {
                dx = (x[i + 1] - x[i]) / 21.;
                dy = (y[j + 1] - y[j]) / 21.;
                for (k = 0; k < 20; k++)
                {
                    xx[k] = x[i] + (dx / 2.) + (k * dx);
                    yy[k] = y[j] + (dy / 2.) + (k * dy);
                }
                cnt = 0;
                for (k = 0; k < 20; k++)
                {
                    for (l = 0; l < 20; l++)
                    {
                        dist = sqrt((xx[k] - xcenter) *
                                    (xx[k] - xcenter) +
                                    (yy[l] - ycenter) *
                                    (yy[l] - ycenter));
                        cnt += (dist < .2) ? 1 : 0;
                    }
                }
                matlist[j * NX + i] = -(mixlen + 1);
                mix_mat[mixlen] = 2;
                mix_mat[mixlen + 1] = 3;
                mix_vf[mixlen] = 1. - (((float)cnt) / 400.);
                mix_vf[mixlen + 1] = ((float)cnt) / 400.;
                mixlen += 2;
            }
        }
    }

    cycle = 48;
    time = 4.8;
    dtime = 4.8;

    delta_x = NX / nblocks_x;
    delta_y = NY / nblocks_y;

    coords[0] = x2;
    coords[1] = y2;
    dims[0] = delta_x + 1;
    dims[1] = delta_y + 1;
    zdims[0] = delta_x;
    zdims[1] = delta_y;
    dims2[0] = delta_x;
    dims2[1] = delta_y;

    /*
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y; block++)
    {
        fprintf(stdout, "\t%s\n", dirnames[block]);

        /*
         * Now extract the data for this block.
         */

        base_x = (block % nblocks_x) * delta_x;
        base_y = (block / nblocks_x) * delta_y;

        for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
            x2[i] = x[n_x];
        for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
            y2[j] = y[n_y];

        for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
            for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
            {
                u2[j * (delta_x + 1) + i] = u[n_y * (NX + 1) + n_x];
                v2[j * (delta_x + 1) + i] = v[n_y * (NX + 1) + n_x];
            }

        mixlen2 = 0;
        for (j = 0, n_y = base_y; j < delta_y; j++, n_y++)
            for (i = 0, n_x = base_x; i < delta_x; i++, n_x++)
            {
                d2[j * delta_x + i] = d[n_y * NX + n_x];
                p2[j * delta_x + i] = p[n_y * NX + n_x];

                if (matlist[n_y * NX + n_x] < 0)
                {
                    mixlen = -matlist[n_y * NX + n_x] - 1;

                    matlist2[j * delta_x + i] = -(mixlen2 + 1);
                    mix_mat2[mixlen2] = mix_mat[mixlen];
                    mix_mat2[mixlen2 + 1] = mix_mat[mixlen + 1];
                    mix_next2[mixlen2] = mixlen2 + 2;
                    mix_next2[mixlen2 + 1] = 0;
                    mix_zone2[mixlen2] = j * delta_x + i;
                    mix_zone2[mixlen2 + 1] = j * delta_x + i;
                    mix_vf2[mixlen2] = mix_vf[mixlen];
                    mix_vf2[mixlen2 + 1] = mix_vf[mixlen + 1];
                    mixlen2 += 2;
                } else
                    matlist2[j * delta_x + i] = matlist[n_y * NX + n_x];
            }

        if (DBSetDir(dbfile, dirnames[block]) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n",
                    dirnames[block]);
            return;
        }                       /* if */

        /* Write out the variables. */
        optlist = DBMakeOptlist(11);
        DBAddOption(optlist, DBOPT_CYCLE, &cycle);
        DBAddOption(optlist, DBOPT_TIME, &time);
        DBAddOption(optlist, DBOPT_DTIME, &dtime);
        DBAddOption(optlist, DBOPT_XLABEL, "X Axis");
        DBAddOption(optlist, DBOPT_YLABEL, "Y Axis");
        DBAddOption(optlist, DBOPT_XUNITS, "cm");
        DBAddOption(optlist, DBOPT_YUNITS, "cm");
        DBAddOption(optlist, DBOPT_MATNAMES, matnames);

        /* populate optional data arrays */
        put_extents(x2,dims[0],varextents[0],block);
        put_extents(y2,dims[1],varextents[1],block);
        has_external_zones[block] = 0;
        if ((varextents[0][2*block] <= 0.0) ||
            (varextents[1][2*block] <= 0.0) ||
            (varextents[0][2*block+1] >= 1.0) ||
            (varextents[1][2*block+1] >= 1.0))
            has_external_zones[block] = 1;
        zonecounts[block] = (dims[0]-1)*(dims[1]-1);
        DBPutQuadmesh(dbfile, meshname, coordnames, coords, dims, ndims,
                      DB_FLOAT, DB_COLLINEAR, optlist);

        put_extents(d2,(dims[0]-1)*(dims[1]-1),varextents[3],block);
        DBPutQuadvar1(dbfile, var1name, meshname, d2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(p2,(dims[0]-1)*(dims[1]-1),varextents[4],block);
        DBPutQuadvar1(dbfile, var2name, meshname, p2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(u2,dims[0]*dims[1],varextents[5],block);
        DBPutQuadvar1(dbfile, var3name, meshname, u2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(v2,dims[0]*dims[1],varextents[6],block);
        DBPutQuadvar1(dbfile, var4name, meshname, v2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        matcounts[block] = count_mats(dims2[0]*dims2[1],matlist2,matlists[block]);
        mixlens[block] = mixlen2;
        DBPutMaterial(dbfile, matname, meshname, nmats, matnos,
                      matlist2, dims2, ndims, mix_next2, mix_mat2,
                      mix_zone2, mix_vf2, mixlen2, DB_FLOAT, optlist);

        DBFreeOptlist(optlist);

        if (DBSetDir(dbfile, "..") == -1)
        {
            fprintf(stderr, "Could not return to base directory\n");
            return;
        }                       /* if */
    }                           /* for */
    for(i=0;i<nmats;i++)
        FREE(matnames[i]);
    FREE(matnames);

}                               /* build_block_rect2d */

/*-------------------------------------------------------------------------
 * Function:    build_block_curv2d
 *
 * Purpose:     Build a 2-d curvilinear mesh and place it in the open
 *              database.
 *
 * Return:      Success:        void
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *    Katherine Price, Aug 4, 1995
 *    Modified function to output blocks.
 *
 *    Robb Matzke, Sun Dec 18 17:39:27 EST 1994
 *    Fixed memory leak.
 *
 *    Eric Brugger, Mon Mar 2  20:46:27 PDT 1998
 *    Changed the declaration of dirnames to use the constant STRLEN.
 *
 *-------------------------------------------------------------------------
 */
void
build_block_curv2d(DBfile *dbfile, char dirnames[MAXBLOCKS][STRLEN],
                   int nblocks_x, int nblocks_y)
{
    int             cycle;
    float           time;
    double          dtime;
    char           *coordnames[3];
    int             ndims;
    int             dims[3], zdims[3];
    float          *coords[3];
    float           x[(NX + 1) * (NY + 1)], y[(NX + 1) * (NY + 1)];

    char           *meshname, *var1name, *var2name, *var3name, *var4name, *matname;
    float           d[NX * NY], p[NX * NY], u[(NX + 1) * (NY + 1)], v[(NX + 1) * (NY + 1)];

    int             nmats;
    int             matnos[3];
    int             matlist[NX * NY];
    int             dims2[3];
    int             mixlen;
    int             mix_next[NX * NY], mix_mat[NX * NY], mix_zone[NX * NY];
    float           mix_vf[NX * NY];

    DBoptlist      *optlist = NULL;

    int             i, j;
    float           xave, yave;
    float           xcenter, ycenter;
    float           theta, dtheta;
    float           r, dr;
    float           dist;

    int             block;
    int             delta_x, delta_y;
    int             base_x, base_y;
    int             n_x, n_y;

    float           x2[(NX + 1) * (NY + 1)], y2[(NX + 1) * (NY + 1)];
    float           d2[NX * NY], p2[NX * NY], u2[(NX + 1) * (NY + 1)], v2[(NX + 1) * (NY + 1)];
    int             matlist2[NX * NY];

    /* 
     * Create the mesh.
     */
    meshname = "mesh1";
    coordnames[0] = "xcoords";
    coordnames[1] = "ycoords";
    coordnames[2] = "zcoords";
    coords[0] = x;
    coords[1] = y;
    ndims = 2;
    dims[0] = NX + 1;
    dims[1] = NY + 1;
    dtheta = (180. / NX) * (3.1415926 / 180.);
    dr = 3. / NY;
    theta = 0;
    for (i = 0; i < NX + 1; i++)
    {
        r = 2.;
        for (j = 0; j < NY + 1; j++)
        {
            x[j * (NX + 1) + i] = r * cos(theta);
            y[j * (NX + 1) + i] = r * sin(theta);
            r += dr;
        }
        theta += dtheta;
    }

    /* 
     * Create the density and pressure arrays.
     */
    var1name = "d";
    var2name = "p";
    xcenter = 0.;
    ycenter = 0.;
    zdims[0] = NX;
    zdims[1] = NY;
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NY; j++)
        {
            xave = (x[(j) * (NX + 1) + i] + x[(j) * (NX + 1) + i + 1] +
                    x[(j + 1) * (NX + 1) + i + 1] + x[(j + 1) * (NX + 1) + i]) / 4.;
            yave = (y[(j) * (NX + 1) + i] + y[(j) * (NX + 1) + i + 1] +
                    y[(j + 1) * (NX + 1) + i + 1] + y[(j + 1) * (NX + 1) + i]) / 4.;
            dist = sqrt((xave - xcenter) * (xave - xcenter) +
                        (yave - ycenter) * (yave - ycenter));
            d[j * NX + i] = dist;
            p[j * NX + i] = 1. / (dist + .0001);
        }
    }

    /* 
     * Create the velocity component arrays. Note that the indexing
     * on the x and y coordinates is for rectilinear meshes. It
     * generates a nice vector field.
     */
    var3name = "u";
    var4name = "v";
    xcenter = 0.;
    ycenter = 0.;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                        (y[j] - ycenter) * (y[j] - ycenter));
            u[j * (NX + 1) + i] = (x[i] - xcenter) / dist;
            v[j * (NX + 1) + i] = (y[j] - ycenter) / dist;
        }
    }

    /* 
     * Create the material array.
     */
    matname = "mat1";
    nmats = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;
    dims2[0] = NX;
    dims2[1] = NY;
    mixlen = 0;

    /* 
     * Put in the material in 3 shells.
     */
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < 10; j++)
        {
            matlist[j * NX + i] = 1;
        }
        for (j = 10; j < 20; j++)
        {
            matlist[j * NX + i] = 2;
        }
        for (j = 20; j < NY; j++)
        {
            matlist[j * NX + i] = 3;
        }
    }

    delta_x = NX / nblocks_x;
    delta_y = NY / nblocks_y;

    coords[0] = x2;
    coords[1] = y2;
    dims[0] = delta_x + 1;
    dims[1] = delta_y + 1;
    zdims[0] = delta_x;
    zdims[1] = delta_y;
    dims2[0] = delta_x;
    dims2[1] = delta_y;

    /* 
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y; block++)
    {
        fprintf(stdout, "\t%s\n", dirnames[block]);

        /* 
         * Now extract the data for this block.
         */

        base_x = (block % nblocks_x) * delta_x;
        base_y = (block / nblocks_x) * delta_y;

        for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
            for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
            {
                x2[j * (delta_x + 1) + i] = x[n_y * (NX + 1) + n_x];
                y2[j * (delta_x + 1) + i] = y[n_y * (NX + 1) + n_x];
                u2[j * (delta_x + 1) + i] = u[n_y * (NX + 1) + n_x];
                v2[j * (delta_x + 1) + i] = v[n_y * (NX + 1) + n_x];
            }

        for (j = 0, n_y = base_y; j < delta_y; j++, n_y++)
            for (i = 0, n_x = base_x; i < delta_x; i++, n_x++)
            {
                d2[j * delta_x + i] = d[n_y * NX + n_x];
                p2[j * delta_x + i] = p[n_y * NX + n_x];
                matlist2[j * delta_x + i] = matlist[n_y * NX + n_x];
            }

        if (DBSetDir(dbfile, dirnames[block]) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n",
                    dirnames[block]);
            return;
        }                              /* if */
        /* Write out the variables. */
        cycle = 48;
        time = 4.8;
        dtime = 4.8;

        optlist = DBMakeOptlist(10);
        DBAddOption(optlist, DBOPT_CYCLE, &cycle);
        DBAddOption(optlist, DBOPT_TIME, &time);
        DBAddOption(optlist, DBOPT_DTIME, &dtime);
        DBAddOption(optlist, DBOPT_XLABEL, "X Axis");
        DBAddOption(optlist, DBOPT_YLABEL, "Y Axis");
        DBAddOption(optlist, DBOPT_XUNITS, "cm");
        DBAddOption(optlist, DBOPT_YUNITS, "cm");

        put_extents(x2,dims[0]*dims[1],varextents[0],block);
        put_extents(y2,dims[0]*dims[1],varextents[1],block);
        has_external_zones[block] = 1;
        zonecounts[block] = (dims[0]-1)*(dims[1]-1);
        DBPutQuadmesh(dbfile, meshname, coordnames, coords, dims, ndims,
                      DB_FLOAT, DB_NONCOLLINEAR, optlist);

        put_extents(d2,(dims[0]-1)*(dims[1]-1),varextents[3],block);
        DBPutQuadvar1(dbfile, var1name, meshname, d2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(p2,(dims[0]-1)*(dims[1]-1),varextents[4],block);
        DBPutQuadvar1(dbfile, var2name, meshname, p2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(u2,dims[0]*dims[1],varextents[5],block);
        DBPutQuadvar1(dbfile, var3name, meshname, u2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(v2,dims[0]*dims[1],varextents[6],block);
        DBPutQuadvar1(dbfile, var4name, meshname, v2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        matcounts[block] = count_mats(dims2[0]*dims2[1],matlist2,matlists[block]);
        mixlens[block] = mixlen;
        DBPutMaterial(dbfile, matname, meshname, nmats, matnos,
                      matlist2, dims2, ndims, mix_next, mix_mat, mix_zone,
                      mix_vf, mixlen, DB_FLOAT, optlist);

        DBFreeOptlist(optlist);

        if (DBSetDir(dbfile, "..") == -1)
        {
            fprintf(stderr, "Could not return to base directory\n");
            return;
        }                              /* if */
    }                                  /* for */

}                                      /* build_block_curv2d */

/*-------------------------------------------------------------------------
 * Function:    build_block_point2d
 *
 * Purpose:     Build a 2-d point mesh and place it in the open
 *              database.
 *
 * Return:      Success:        void
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
void
build_block_point2d(DBfile *dbfile, char dirnames[MAXBLOCKS][STRLEN],
                    int nblocks_x, int nblocks_y)
{
    int             cycle;
    float           time;
    double          dtime;
    float          *coords[3];
    float           x[(NX + 1) * (NY + 1)], y[(NX + 1) * (NY + 1)];

    char           *meshname, *var1name, *var2name, *var3name, *var4name;
    float           d[(NX + 1) * (NY + 1)], p[(NX + 1) * (NY + 1)];
    float           u[(NX + 1) * (NY + 1)], v[(NX + 1) * (NY + 1)];

    DBoptlist      *optlist = NULL;

    int             i, j;
    float           xcenter, ycenter;
    float           theta, dtheta;
    float           r, dr;
    float           dist;

    int             block;
    int             delta_x, delta_y;
    int             base_x, base_y;
    int             n_x, n_y;
    int             npts;
    float          *vars[1];

    float           x2[(NX + 1) * (NY + 1)], y2[(NX + 1) * (NY + 1)];
    float           d2[(NX + 1) * (NY + 1)], p2[(NX + 1) * (NY + 1)];
    float           u2[(NX + 1) * (NY + 1)], v2[(NX + 1) * (NY + 1)];

    /* 
     * Create the mesh.
     */
    meshname = "mesh1";
    coords[0] = x;
    coords[1] = y;
    dtheta = (180. / NX) * (3.1415926 / 180.);
    dr = 3. / NY;
    theta = 0;
    for (i = 0; i < NX + 1; i++)
    {
        r = 2.;
        for (j = 0; j < NY + 1; j++)
        {
            x[j * (NX + 1) + i] = r * cos(theta);
            y[j * (NX + 1) + i] = r * sin(theta);
            r += dr;
        }
        theta += dtheta;
    }

    /* 
     * Create the density and pressure arrays.
     */
    var1name = "d";
    var2name = "p";
    xcenter = 0.;
    ycenter = 0.;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            dist = sqrt((x[j * (NX + 1) + i] - xcenter) * (x[j * (NX + 1) + i] - xcenter) +
                        (y[j * (NX + 1) + i] - ycenter) * (y[j * (NX + 1) + i] - ycenter));
            d[j * (NX + 1) + i] = dist;
            p[j * (NX + 1) + i] = 1. / (dist + .0001);
        }
    }

    /* 
     * Create the velocity component arrays.
     */
    var3name = "u";
    var4name = "v";
    xcenter = 0.;
    ycenter = 0.;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                        (y[j] - ycenter) * (y[j] - ycenter));
            u[j * (NX + 1) + i] = (x[i] - xcenter) / dist;
            v[j * (NX + 1) + i] = (y[j] - ycenter) / dist;
        }
    }

    delta_x = NX / nblocks_x;
    delta_y = NY / nblocks_y;

    coords[0] = x2;
    coords[1] = y2;

    /* 
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y; block++)
    {
        fprintf(stdout, "\t%s\n", dirnames[block]);

        /* 
         * Now extract the data for this block.
         */

        base_x = (block % nblocks_x) * delta_x;
        base_y = (block / nblocks_x) * delta_y;

        for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
            for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
            {
                x2[j * (delta_x + 1) + i] = x[n_y * (NX + 1) + n_x];
                y2[j * (delta_x + 1) + i] = y[n_y * (NX + 1) + n_x];
                d2[j * (delta_x + 1) + i] = d[n_y * (NX + 1) + n_x];
                p2[j * (delta_x + 1) + i] = p[n_y * (NX + 1) + n_x];
                u2[j * (delta_x + 1) + i] = u[n_y * (NX + 1) + n_x];
                v2[j * (delta_x + 1) + i] = v[n_y * (NX + 1) + n_x];
            }

        if (DBSetDir(dbfile, dirnames[block]) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n",
                    dirnames[block]);
            return;
        }                              /* if */
        /* Write out the variables. */
        cycle = 48;
        time = 4.8;
        dtime = 4.8;

        npts = (delta_x + 1) * (delta_y + 1);

        optlist = DBMakeOptlist(10);
        DBAddOption(optlist, DBOPT_CYCLE, &cycle);
        DBAddOption(optlist, DBOPT_TIME, &time);
        DBAddOption(optlist, DBOPT_DTIME, &dtime);
        DBAddOption(optlist, DBOPT_XLABEL, "X Axis");
        DBAddOption(optlist, DBOPT_YLABEL, "Y Axis");
        DBAddOption(optlist, DBOPT_XUNITS, "cm");
        DBAddOption(optlist, DBOPT_YUNITS, "cm");

        put_extents(x2,npts,varextents[0],block);
        put_extents(y2,npts,varextents[1],block);
        zonecounts[block] = 0;
        DBPutPointmesh(dbfile, meshname, 2, coords, npts, DB_FLOAT, optlist);

        put_extents(d2,npts,varextents[3],block);
        vars[0] = d2;
        DBPutPointvar(dbfile, var1name, meshname, 1, vars, npts, DB_FLOAT,
                      optlist);

        put_extents(p2,npts,varextents[4],block);
        vars[0] = p2;
        DBPutPointvar(dbfile, var2name, meshname, 1, vars, npts, DB_FLOAT,
                      optlist);

        put_extents(u2,npts,varextents[5],block);
        vars[0] = u2;
        DBPutPointvar(dbfile, var3name, meshname, 1, vars, npts, DB_FLOAT,
                      optlist);

        put_extents(v2,npts,varextents[6],block);
        vars[0] = v2;
        DBPutPointvar(dbfile, var4name, meshname, 1, vars, npts, DB_FLOAT,
                      optlist);

        DBFreeOptlist(optlist);

        if (DBSetDir(dbfile, "..") == -1)
        {
            fprintf(stderr, "Could not return to base directory\n");
            return;
        }                              /* if */
    }                                  /* for */

}                                      /* build_block_point2d */

/*-------------------------------------------------------------------------
 * Function:    build_block_rect3d
 *
 * Purpose:     Build a 3-d rectilinear mesh and add it to the open database.
 *
 * Return:      Success:
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *
 *    Katherine Price, Aug 4, 1995
 *    Modified function to output blocks.
 *
 *    Robb Matzke, Sun Dec 18 17:39:58 EST 1994
 *    Fixed memory leak.
 *
 *    Eric Brugger, Fri Oct 17 17:09:00 PDT 1997
 *    I modified the routine to output 3 materials instead of 2 and
 *    to use the routines fill_rect3d_bkgr and fill_rect3d_mat to
 *    fill the material arrays.
 *
 *    Eric Brugger, Mon Mar 2  20:46:27 PDT 1998
 *    I increased the size of the mixed material arrays to avoid a trap
 *    of a memory write condition.
 *
 *    Sean Ahern, Thu Jul  2 11:04:30 PDT 1998
 *    Fixed an indexing problem.
 *
 *-------------------------------------------------------------------------
 */
void
build_block_rect3d(DBfile *dbfile, char dirnames[MAXBLOCKS][STRLEN],
                   int nblocks_x, int nblocks_y, int nblocks_z)
{
    int             cycle;
    float           time;
    double          dtime;
    char           *coordnames[3];
    int             ndims;
    int             dims[3], zdims[3];
    float          *coords[3];
    float           x[NX + 1], y[NY + 1], z[NZ + 1];

    char           *meshname, *var1name, *var2name, *var3name, *var4name;
    char           *var5name, *matname;
    float           d[NX * NY * NZ], p[NX * NY * NZ];
    float           u[(NX + 1) * (NY + 1) * (NZ + 1)], v[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           w[(NX + 1) * (NY + 1) * (NZ + 1)];

    int             nmats;
    int             matnos[3];
    int             matlist[NX * NY * NZ];
    int             dims2[3];
    int             mixlen;
    int             mix_next[MIXMAX], mix_mat[MIXMAX], mix_zone[MIXMAX];
    float           mix_vf[MIXMAX];

    DBoptlist      *optlist = NULL;

    int             i, j, k;
    float           xave, yave, zave;
    float           xcenter, ycenter, zcenter;
    float           dist;

    int             block;
    int             delta_x, delta_y, delta_z;
    int             base_x, base_y, base_z;
    int             n_x, n_y, n_z;

    float           x2[NX + 1], y2[NY + 1], z2[NZ + 1];
    float           d2[NX * NY * NZ], p2[NX * NY * NZ];
    float           u2[(NX + 1) * (NY + 1) * (NZ + 1)], v2[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           w2[(NX + 1) * (NY + 1) * (NZ + 1)];
    int             matlist2[NX * NY * NZ];
    int             mixlen2;
    int             mix_next2[MIXMAX], mix_mat2[MIXMAX], mix_zone2[MIXMAX];
    float           mix_vf2[MIXMAX];

    /* 
     * Create the mesh.
     */
    meshname = "mesh1";
    coordnames[0] = "xcoords";
    coordnames[1] = "ycoords";
    coordnames[2] = "zcoords";
    coords[0] = x;
    coords[1] = y;
    coords[2] = z;
    ndims = 3;
    dims[0] = NX + 1;
    dims[1] = NY + 1;
    dims[2] = NZ + 1;
    for (i = 0; i < NX + 1; i++)
        x[i] = i * (1. / NX);
    for (i = 0; i < NY + 1; i++)
        y[i] = i * (1. / NY);
    for (i = 0; i < NZ + 1; i++)
        z[i] = i * (1. / NZ);

    /* 
     * Create the density and pressure arrays.
     */
    var1name = "d";
    var2name = "p";
    xcenter = .5;
    ycenter = .5;
    zcenter = .5;
    zdims[0] = NX;
    zdims[1] = NY;
    zdims[2] = NZ;
    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NY; j++)
        {
            for (k = 0; k < NZ; k++)
            {
                xave = (x[i] + x[i + 1]) / 2.;
                yave = (y[j] + y[j + 1]) / 2.;
                zave = (z[k] + z[k + 1]) / 2.;
                dist = sqrt((xave - xcenter) * (xave - xcenter) +
                            (yave - ycenter) * (yave - ycenter) +
                            (zave - zcenter) * (zave - zcenter));
                d[k * NX * NY + j * NX + i] = dist;
                p[k * NX * NY + j * NX + i] = 1. / (dist + .0001);
            }
        }
    }

    /* 
     * Create the velocity component arrays.
     */
    var3name = "u";
    var4name = "v";
    var5name = "w";
    xcenter = .5001;
    ycenter = .5001;
    zcenter = .5001;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            for (k = 0; k < NZ + 1; k++)
            {
                dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                            (y[j] - ycenter) * (y[j] - ycenter) +
                            (z[k] - zcenter) * (z[k] - zcenter));
                u[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] = (x[i] - xcenter) / dist;
                v[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] = (y[j] - ycenter) / dist;
                w[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] = (z[k] - zcenter) / dist;
            }
        }
    }

    /* 
     * Create the material array.
     */
    matname = "mat1";
    nmats = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;
    dims2[0] = NX;
    dims2[1] = NY;
    dims2[2] = NZ;
    mixlen = 0;

    /* 
     * Put in the material for the entire mesh.
     */
    fill_rect3d_bkgr(matlist, NX, NY, NZ, 1);

    mixlen = 0;
    fill_rect3d_mat(x, y, z, matlist, NX, NY, NZ, mix_next,
                  mix_mat, mix_zone, mix_vf, &mixlen, 2, .5, .5, .5, .6);
    fill_rect3d_mat(x, y, z, matlist, NX, NY, NZ, mix_next,
                  mix_mat, mix_zone, mix_vf, &mixlen, 3, .5, .5, .5, .4);
    if (mixlen > MIXMAX)
    {
        printf("memory overwrite: mixlen = %d > %d\n", mixlen, MIXMAX);
        exit(-1);
    }
    /* 
     * Now extract the data for this block.
     */

    delta_x = NX / nblocks_x;
    delta_y = NY / nblocks_y;
    delta_z = NZ / nblocks_z;

    coords[0] = x2;
    coords[1] = y2;
    coords[2] = z2;
    dims[0] = delta_x + 1;
    dims[1] = delta_y + 1;
    dims[2] = delta_z + 1;
    zdims[0] = delta_x;
    zdims[1] = delta_y;
    zdims[2] = delta_z;
    dims2[0] = delta_x;
    dims2[1] = delta_y;
    dims2[2] = delta_z;

    /* 
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y * nblocks_z; block++)
    {
        fprintf(stdout, "\t%s\n", dirnames[block]);

        /* 
         * Now extract the data for this block.
         */

        base_x = (block % nblocks_x) * delta_x;
        base_y = ((block % (nblocks_x * nblocks_y)) / nblocks_x) * delta_y;
        base_z = (block / (nblocks_x * nblocks_y)) * delta_z;

        for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
            x2[i] = x[n_x];
        for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
            y2[j] = y[n_y];
        for (k = 0, n_z = base_z; k < delta_z + 1; k++, n_z++)
            z2[k] = z[n_z];

        for (k = 0, n_z = base_z; k < delta_z + 1; k++, n_z++)
            for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
                for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
                {
                    u2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        u[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    v2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        v[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    w2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        w[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                }

        mixlen2 = 0;
        for (k = 0, n_z = base_z; k < delta_z; k++, n_z++)
            for (j = 0, n_y = base_y; j < delta_y; j++, n_y++)
                for (i = 0, n_x = base_x; i < delta_x; i++, n_x++)
                {
                    d2[k * delta_x * delta_y + j * delta_x + i] =
                        d[n_z * NX * NY + n_y * NX + n_x];
                    p2[k * delta_x * delta_y + j * delta_x + i] =
                        p[n_z * NX * NY + n_y * NX + n_x];

                    if (matlist[n_z * NX * NY + n_y * NX + n_x] < 0)
                    {
                        mixlen = -matlist[n_z * NX * NY + n_y * NX + n_x] - 1;

                        matlist2[k * delta_x * delta_y + j * delta_x + i]
                            = -(mixlen2 + 1);
                        mix_mat2[mixlen2] = mix_mat[mixlen];
                        mix_mat2[mixlen2 + 1] = mix_mat[mixlen + 1];
                        mix_next2[mixlen2] = mixlen2 + 2;
                        mix_next2[mixlen2 + 1] = 0;
                        mix_zone2[mixlen2]
                            = k * delta_x * delta_y + j * delta_x + i;
                        mix_zone2[mixlen2 + 1]
                            = k * delta_x * delta_y + j * delta_x + i;
                        mix_vf2[mixlen2] = mix_vf[mixlen];
                        mix_vf2[mixlen2 + 1] = mix_vf[mixlen + 1];
                        mixlen2 += 2;
                    } else
                        matlist2[k * delta_x * delta_y + j * delta_x + i]
                            = matlist[n_z * NX * NY + n_y * NX + n_x];
                }

        if (DBSetDir(dbfile, dirnames[block]) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n",
                    dirnames[block]);
            return;
        }                              /* if */
        /* Write out the variables. */
        cycle = 48;
        time = 4.8;
        dtime = 4.8;

        optlist = DBMakeOptlist(10);
        DBAddOption(optlist, DBOPT_CYCLE, &cycle);
        DBAddOption(optlist, DBOPT_TIME, &time);
        DBAddOption(optlist, DBOPT_DTIME, &dtime);
        DBAddOption(optlist, DBOPT_XLABEL, "X Axis");
        DBAddOption(optlist, DBOPT_YLABEL, "Y Axis");
        DBAddOption(optlist, DBOPT_ZLABEL, "Z Axis");
        DBAddOption(optlist, DBOPT_XUNITS, "cm");
        DBAddOption(optlist, DBOPT_YUNITS, "cm");
        DBAddOption(optlist, DBOPT_ZUNITS, "cm");

        /* populate varextetnts optional data array */
        put_extents(x2,dims[0],varextents[0],block);
        put_extents(y2,dims[1],varextents[1],block);
        put_extents(z2,dims[2],varextents[2],block);

        /* populate 'has_external_zones' optional data array */
        has_external_zones[block] = 0;
        if ((varextents[0][2*block] <= 0.0) ||
            (varextents[1][2*block] <= 0.0) ||
            (varextents[2][2*block] <= 0.0) ||
            (varextents[0][2*block+1] >= 1.0) ||
            (varextents[1][2*block+1] >= 1.0) ||
            (varextents[2][2*block+1] >= 1.0))
            has_external_zones[block] = 1;

        zonecounts[block] = (dims[0]-1)*(dims[1]-1)*(dims[2]-1);
        DBPutQuadmesh(dbfile, meshname, coordnames, coords, dims, ndims,
                      DB_FLOAT, DB_COLLINEAR, optlist);

        put_extents(d2,(dims[0]-1)*(dims[1]-1)*(dims[2]-1),varextents[3],block);
        DBPutQuadvar1(dbfile, var1name, meshname, d2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(p2,(dims[0]-1)*(dims[1]-1)*(dims[2]-1),varextents[4],block);
        DBPutQuadvar1(dbfile, var2name, meshname, p2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(u2,dims[0]*dims[1]*dims[2],varextents[5],block);
        DBPutQuadvar1(dbfile, var3name, meshname, u2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(v2,dims[0]*dims[1]*dims[2],varextents[6],block);
        DBPutQuadvar1(dbfile, var4name, meshname, v2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(w2,dims[0]*dims[1]*dims[2],varextents[7],block);
        DBPutQuadvar1(dbfile, var5name, meshname, w2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        matcounts[block] = count_mats(dims2[0]*dims2[1]*dims2[2],matlist2,matlists[block]);
        mixlens[block] = mixlen2;
        DBPutMaterial(dbfile, matname, meshname, nmats, matnos,
                  matlist2, dims2, ndims, mix_next2, mix_mat2, mix_zone2,
                      mix_vf2, mixlen2, DB_FLOAT, optlist);

        DBFreeOptlist(optlist);

        if (DBSetDir(dbfile, "..") == -1)
        {
            fprintf(stderr, "Could not return to base directory\n");
            return;
        }                              /* if */
    }                                  /* for */

}                                      /* build_block_rect3d */

/*-------------------------------------------------------------------------
 * Function:    build_block_ucd3d
 *
 * Purpose:     Build a 3-d UCD mesh and add it to the open database.
 *
 * Return:      Success:        void
 *
 *              Failure:
 *
 * Programmer:
 *
 * Modifications:
 *    Mark Miller, Tue Oct  6 09:46:48 PDT 1998
 *    fixing node numbering in ucd3d stuff
 *
 *    Katherine Price, Aug 4, 1995
 *    Modified function to output blocks.
 *
 *    Robb Matzke, Sun Dec 18 17:40:58 EST 1994
 *    Fixed memory leak.
 *
 *    Eric Brugger, Fri Oct 17 17:09:00 PDT 1997
 *    I modified the routine to output more blocks, mixed material zones,
 *    and ghost zones.
 *
 *    Eric Brugger, Thu Oct 23 16:37:03 PDT 1997
 *    I corrected a bug where the max_index passed to the routine
 *    DBCalcExternalFacelist2 was 1 too large.
 *
 *    Eric Brugger, Fri Mar 12 16:01:44 PST 1999
 *    I modified the routine to use the new interface to
 *    DBCalcExternalFacelist2.
 *
 *-------------------------------------------------------------------------
 */
void
build_block_ucd3d(DBfile *dbfile, char dirnames[MAXBLOCKS][STRLEN],
                  int nblocks_x, int nblocks_y, int nblocks_z)
{
#undef  NX
#define NX 30
#undef  NY
#define NY 40
#undef  NZ
#define NZ 30

    int             cycle;
    float           time;
    double          dtime;
    char           *coordnames[3];
    float          *coords[3];
    float           x[(NX + 1) * (NY + 1) * (NZ + 1)], y[(NX + 1) * (NY + 1) * (NZ + 1)],
                    z[(NX + 1) * (NY + 1) * (NZ + 1)];
    int             nfaces, nzones, nnodes;
    int             lfacelist, lzonelist;
    int             fshapesize, fshapecnt, zshapetype, zshapesize, zshapecnt;
    int             zonelist[16000];
    int             facelist[10000];
    int             zoneno[2000];

    char           *meshname, *var1name, *var2name, *var3name, *var4name;
    char           *var5name, *matname;
    float          *vars[1];
    char           *varnames[1];
    float           d[(NX + 1) * (NY + 1) * (NZ + 1)], p[(NX + 1) * (NY + 1) * (NZ + 1)],
                    u[(NX + 1) * (NY + 1) * (NZ + 1)], v[(NX + 1) * (NY + 1) * (NZ + 1)],
                    w[(NX + 1) * (NY + 1) * (NZ + 1)];

    int             nmats;
    int             matnos[3];
    int             matlist[NX * NY * NZ];
    int             mixlen;
    int             mix_next[4500], mix_mat[4500], mix_zone[4500];
    float           mix_vf[4500];
    float           xstrip[NX + NY + NZ], ystrip[NX + NY + NZ], zstrip[NX + NY + NZ];

    int             one = 1;
    DBoptlist      *optlist;

    DBfacelist     *fl;

    int             i, j, k;
    int             iz;
    float           xcenter, ycenter;
    float           theta, dtheta;
    float           r, dr;
    float           h, dh;
    float           dist;

    int             block;
    int             delta_x, delta_y, delta_z;
    int             n_x, n_y, n_z;

    int             imin, imax, jmin, jmax, kmin, kmax;
    int             nx, ny, nz;

    float           x2[2646], y2[2646], z2[2646];
    float           d2[2646], p2[2646], u2[2646], v2[2646], w2[2646];
    int             matlist2[2000], ghost[2000];

    int             nreal;
    int             ighost;
    int             itemp;
    int             hi_off;

    DBobject       *obj;

    /* 
     * Create the coordinate arrays for the entire mesh.
     */
    dh = 20. / (float)NX;
    dtheta = (180. / (float)NY) * (3.1415926 / 180.);
    dr = 3. / (float)NZ;
    h = 0.;
    for (i = 0; i < NX + 1; i++)
    {
        theta = 0.;
        for (j = 0; j < NY + 1; j++)
        {
            r = 2.;
            for (k = 0; k < NZ + 1; k++)
            {
                x[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = r * cos(theta);
                y[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = r * sin(theta);
                z[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = h;
                r += dr;
            }
            theta += dtheta;
        }
        h += dh;
    }

    /* 
     * Create the density and pressure arrays for the entire mesh.
     */
    xcenter = 0.;
    ycenter = 0.;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            for (k = 0; k < NZ + 1; k++)
            {
                dist = sqrt((x[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] - xcenter) *
                            (x[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] - xcenter) +
                            (y[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] - ycenter) *
                            (y[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] - ycenter));
                d[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = dist;
                p[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = 1. / (dist + .0001);
            }
        }
    }

    /* 
     * Create the velocity component arrays for the entire mesh.
     */
    xcenter = 0.;
    ycenter = 0.;
    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            for (k = 0; k < NZ + 1; k++)
            {
                dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                            (y[j] - ycenter) * (y[j] - ycenter));
                u[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = (x[i] - xcenter) / dist;
                v[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = (y[j] - ycenter) / dist;
                w[i * (NX + 1) * (NY + 1) + j * (NX + 1) + k] = 0.;
            }
        }
    }

    /* 
     * Put in the material for the entire mesh.
     */
    fill_rect3d_bkgr(matlist, NX, NY, NZ, 1);

    for (i = 0; i < NY; i++)
    {
        xstrip[i] = (float)i;
        ystrip[i] = (float)i;
        zstrip[i] = (float)i;
    }

    mixlen = 0;
    fill_rect3d_mat(xstrip, ystrip, zstrip, matlist, NX, NY, NZ, mix_next,
              mix_mat, mix_zone, mix_vf, &mixlen, 2, 15., 20., 15., 10.);
    fill_rect3d_mat(xstrip, ystrip, zstrip, matlist, NX, NY, NZ, mix_next,
               mix_mat, mix_zone, mix_vf, &mixlen, 3, 15., 20., 15., 5.);
    if (mixlen > 4500)
    {
        printf("memory overwrite: mixlen = %d > 4500\n", mixlen);
        exit(-1);
    }
    /* 
     * Set up variables that are independent of the block number.
     */
    cycle = 48;
    time = 4.8;
    dtime = 4.8;

    meshname = "mesh1";
    coordnames[0] = "xcoords";
    coordnames[1] = "ycoords";
    coordnames[2] = "zcoords";

    var1name = "d";
    var2name = "p";
    var3name = "u";
    var4name = "v";
    var5name = "w";

    matname = "mat1";
    nmats = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;

    /* 
     * Now extract the data for this block.
     */
    delta_x = NX / nblocks_x;
    delta_y = NY / nblocks_y;
    delta_z = NZ / nblocks_z;

    coords[0] = x2;
    coords[1] = y2;
    coords[2] = z2;

    /* 
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y * nblocks_z; block++)
    {
        fprintf(stdout, "\t%s\n", dirnames[block]);

        /* 
         * Now extract the data for this block.
         */
        imin = (block % nblocks_x) * delta_x - 1;
        imax = MIN(imin + delta_x + 3, NX + 1);
        imin = MAX(imin, 0);
        nx = imax - imin;
        jmin = ((block % (nblocks_x * nblocks_y)) / nblocks_x) * delta_y - 1;
        jmax = MIN(jmin + delta_y + 3, NY + 1);
        jmin = MAX(jmin, 0);
        ny = jmax - jmin;
        kmin = (block / (nblocks_x * nblocks_y)) * delta_z - 1;
        kmax = MIN(kmin + delta_z + 3, NZ + 1);
        kmin = MAX(kmin, 0);
        nz = kmax - kmin;

        for (k = 0, n_z = kmin; n_z < kmax; k++, n_z++)
            for (j = 0, n_y = jmin; n_y < jmax; j++, n_y++)
                for (i = 0, n_x = imin; n_x < imax; i++, n_x++)
                {
                    x2[k * nx * ny + j * nx + i] =
                        x[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    y2[k * nx * ny + j * nx + i] =
                        y[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    z2[k * nx * ny + j * nx + i] =
                        z[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    d2[k * nx * ny + j * nx + i] =
                        d[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    p2[k * nx * ny + j * nx + i] =
                        p[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    u2[k * nx * ny + j * nx + i] =
                        u[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    v2[k * nx * ny + j * nx + i] =
                        v[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    w2[k * nx * ny + j * nx + i] =
                        w[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                }

        iz = 0;
        for (k = 0, n_z = kmin; n_z < kmax - 1; k++, n_z++)
            for (j = 0, n_y = jmin; n_y < jmax - 1; j++, n_y++)
                for (i = 0, n_x = imin; n_x < imax - 1; i++, n_x++)
                {
                    zonelist[iz] = (k + 0) * nx * ny + (j + 1) * nx + i + 1;
                    zonelist[iz + 1] = (k + 0) * nx * ny + (j + 0) * nx + i + 1;
                    zonelist[iz + 2] = (k + 1) * nx * ny + (j + 0) * nx + i + 1;
                    zonelist[iz + 3] = (k + 1) * nx * ny + (j + 1) * nx + i + 1;
                    zonelist[iz + 4] = (k + 0) * nx * ny + (j + 1) * nx + i + 0;
                    zonelist[iz + 5] = (k + 0) * nx * ny + (j + 0) * nx + i + 0;
                    zonelist[iz + 6] = (k + 1) * nx * ny + (j + 0) * nx + i + 0;
                    zonelist[iz + 7] = (k + 1) * nx * ny + (j + 1) * nx + i + 0;
                    iz += 8;

                    matlist2[k * (nx - 1) * (ny - 1) + j * (nx - 1) + i] =
                        matlist[n_z * NX * NY + n_y * NX + n_x];

                    if (((k == 0 || n_z == kmax - 2) &&
                         (n_z != 0 && n_z != NZ - 1)) ||
                        ((j == 0 || n_y == jmax - 2) &&
                         (n_y != 0 && n_y != NY - 1)) ||
                        ((i == 0 || n_x == imax - 2) &&
                         (n_x != 0 && n_x != NX - 1)))
                        ghost[k * (nx - 1) * (ny - 1) + j * (nx - 1) + i] = 1;
                    else
                        ghost[k * (nx - 1) * (ny - 1) + j * (nx - 1) + i] = 0;
                }

        /* 
         * Resort the zonelist, matlist so that the ghost zones are at the
         * end.
         */
        nzones = (nx - 1) * (ny - 1) * (nz - 1);
        nreal = nzones;
        for (i = 0; i < nzones; i++)
            nreal -= ghost[i];
        ighost = nzones - 1;
        for (i = 0; i < nreal; i++)
        {
            if (ghost[i] == 1)
            {
                /* 
                 * Find the first non ghost zone.
                 */
                while (ghost[ighost] == 1)
                    ighost--;
                j = ighost;

                itemp = zonelist[i * 8];
                zonelist[i * 8] = zonelist[j * 8];
                zonelist[j * 8] = itemp;
                itemp = zonelist[i * 8 + 1];
                zonelist[i * 8 + 1] = zonelist[j * 8 + 1];
                zonelist[j * 8 + 1] = itemp;
                itemp = zonelist[i * 8 + 2];
                zonelist[i * 8 + 2] = zonelist[j * 8 + 2];
                zonelist[j * 8 + 2] = itemp;
                itemp = zonelist[i * 8 + 3];
                zonelist[i * 8 + 3] = zonelist[j * 8 + 3];
                zonelist[j * 8 + 3] = itemp;
                itemp = zonelist[i * 8 + 4];
                zonelist[i * 8 + 4] = zonelist[j * 8 + 4];
                zonelist[j * 8 + 4] = itemp;
                itemp = zonelist[i * 8 + 5];
                zonelist[i * 8 + 5] = zonelist[j * 8 + 5];
                zonelist[j * 8 + 5] = itemp;
                itemp = zonelist[i * 8 + 6];
                zonelist[i * 8 + 6] = zonelist[j * 8 + 6];
                zonelist[j * 8 + 6] = itemp;
                itemp = zonelist[i * 8 + 7];
                zonelist[i * 8 + 7] = zonelist[j * 8 + 7];
                zonelist[j * 8 + 7] = itemp;

                itemp = matlist2[i];
                matlist2[i] = matlist2[j];
                matlist2[j] = itemp;

                itemp = ghost[i];
                ghost[i] = ghost[j];
                ghost[j] = itemp;
            }
        }

        /* 
         * Calculate the external face list.
         */
        nnodes = nx * ny * nz;
        hi_off = nzones - nreal;

        zshapesize = 8;
        zshapecnt = nzones;
        zshapetype = DB_ZONETYPE_HEX;
        lzonelist = nzones * 8;

        fl = DBCalcExternalFacelist2(zonelist, nnodes, 0, hi_off, 0,
                                 &zshapetype, &zshapesize, &zshapecnt, 1,
                                     matlist2, 0);

        nfaces = fl->nfaces;
        fshapecnt = fl->nfaces;
        fshapesize = 4;
        lfacelist = fl->lnodelist;
        for (i = 0; i < lfacelist; i++)
            facelist[i] = fl->nodelist[i];
        for (i = 0; i < nfaces; i++)
            zoneno[i] = fl->zoneno[i];

        DBFreeFacelist(fl);

        if (DBSetDir(dbfile, dirnames[block]) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n",
                    dirnames[block]);
            return;
        }                              /* if */
        /* Write out the mesh and variables. */
        optlist = DBMakeOptlist(20);
        DBAddOption(optlist, DBOPT_CYCLE, &cycle);
        DBAddOption(optlist, DBOPT_TIME, &time);
        DBAddOption(optlist, DBOPT_DTIME, &dtime);
        DBAddOption(optlist, DBOPT_XLABEL, "X Axis");
        DBAddOption(optlist, DBOPT_YLABEL, "Y Axis");
        DBAddOption(optlist, DBOPT_ZLABEL, "Z Axis");
        DBAddOption(optlist, DBOPT_XUNITS, "cm");
        DBAddOption(optlist, DBOPT_YUNITS, "cm");
        DBAddOption(optlist, DBOPT_ZUNITS, "cm");
        DBAddOption(optlist, DBOPT_HI_OFFSET, &hi_off);

        if (nfaces > 0)
            DBPutFacelist(dbfile, "fl1", nfaces, 3, facelist, lfacelist, 0,
                      zoneno, &fshapesize, &fshapecnt, 1, NULL, NULL, 0);

        /* 
         * Output the zonelist.  This is being done at the object
         * level to add the hi_offset option which can't be output
         * with the DBPutZonelist routine.
         */
        obj = DBMakeObject("zl1", DB_ZONELIST, 20);

        DBAddIntComponent(obj, "ndims", 3);
        DBAddIntComponent(obj, "nzones", nzones);
        DBAddIntComponent(obj, "nshapes", 1);
        DBAddIntComponent(obj, "lnodelist", lzonelist);
        DBAddIntComponent(obj, "origin", 0);
        DBAddIntComponent(obj, "hi_offset", hi_off);
        DBAddVarComponent(obj, "nodelist", "zl1_nodelist");
        DBAddVarComponent(obj, "shapecnt", "zl1_shapecnt");
        DBAddVarComponent(obj, "shapesize", "zl1_shapesize");

        DBWriteObject(dbfile, obj, 0);
        DBFreeObject(obj);

        DBWrite(dbfile, "zl1_nodelist", zonelist, &lzonelist, 1, DB_INT);
        DBWrite(dbfile, "zl1_shapecnt", &zshapecnt, &one, 1, DB_INT);
        DBWrite(dbfile, "zl1_shapesize", &zshapesize, &one, 1, DB_INT);

        /* 
         * Output the rest of the mesh and variables.
         */
        put_extents(x2,nnodes,varextents[0],block);
        put_extents(y2,nnodes,varextents[1],block);
        put_extents(z2,nnodes,varextents[2],block);
        has_external_zones[block] = nfaces ? 1 : 0;
        zonecounts[block] = nzones;
        if (nfaces > 0)
            DBPutUcdmesh(dbfile, meshname, 3, coordnames, coords,
                         nnodes, nzones, "zl1", "fl1", DB_FLOAT, optlist);
        else
            DBPutUcdmesh(dbfile, meshname, 3, coordnames, coords,
                         nnodes, nzones, "zl1", NULL, DB_FLOAT, optlist);

        put_extents(d2,nzones,varextents[3],block);
        vars[0] = d2;
        varnames[0] = var1name;
        DBPutUcdvar(dbfile, var1name, meshname, 1, varnames, vars,
                    nzones, NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(p2,nzones,varextents[4],block);
        vars[0] = p2;
        varnames[0] = var2name;
        DBPutUcdvar(dbfile, var2name, meshname, 1, varnames, vars,
                    nzones, NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(u2,nnodes,varextents[5],block);
        vars[0] = u2;
        varnames[0] = var3name;
        DBPutUcdvar(dbfile, var3name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(v2,nnodes,varextents[6],block);
        vars[0] = v2;
        varnames[0] = var4name;
        DBPutUcdvar(dbfile, var4name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(w2,nnodes,varextents[7],block);
        vars[0] = w2;
        varnames[0] = var5name;
        DBPutUcdvar(dbfile, var5name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        matcounts[block] = count_mats(nzones,matlist2,matlists[block]);
        mixlens[block] = mixlen;
        DBPutMaterial(dbfile, matname, meshname, nmats, matnos,
                      matlist2, &nzones, 1, mix_next, mix_mat, mix_zone,
                      mix_vf, mixlen, DB_FLOAT, optlist);

        DBFreeOptlist(optlist);

        if (DBSetDir(dbfile, "..") == -1)
        {
            fprintf(stderr, "Could not return to base directory\n");
            return;
        }                              /* if */
    }                                  /* for */

}                                      /* build_block_ucd3d */

/*-------------------------------------------------------------------------
 * Function:    build_curv3d
 *
 * Purpose:     Build a 3-d Curvillinear  mesh and add it to the open
 *              database.
 *
 * Return:      Success:        void
 *
 *              Failure:
 *
 * Programmer:  Tony L. Jones
 *              May 30, 1995
 *
 * Modifications:
 *    Tony Jones      June 15, 1995
 *    Density and Pressure calculation was in err.  Previous
 *    algorithm was passing non-existent values to the mentioned
 *    arrays.  Problem fixed by decrementing the max loop index.
 *
 *    Jeremy Meredith, Tue Mar 28 09:38:55 PST 2000
 *    Reversed the angular coordinates so they conform to the right
 *    hand rule.  AGAIN.
 * 
 *-------------------------------------------------------------------------
 */
void
build_block_curv3d(DBfile *dbfile, char dirnames[MAXBLOCKS][STRLEN],
                   int nblocks_x, int nblocks_y, int nblocks_z)
{
#undef  NX
#define NX 30
#undef  NY
#define NY 40
#undef  NZ
#define NZ 30

    int             cycle;
    float           time;
    double          dtime;
    char           *coordnames[3];
    float          *coords[3];

    float           x[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           y[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           z[(NX + 1) * (NY + 1) * (NZ + 1)];

    int             ndims, zdims[3];
    int             dims[3], dims2[3];

    char           *meshname, *var1name, *var2name, *var3name, *var4name;
    char           *var5name, *matname;

    float           d[NX * NY * NZ], p[NX * NY * NZ];
    float           u[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           v[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           w[(NX + 1) * (NY + 1) * (NZ + 1)];

    int             nmats;
    int             matnos[3];
    int             matlist[NX * NY * NZ];
    int             mixlen;
    int             mix_next[NX * NY * NZ], mix_mat[NX * NY * NZ];
    int             mix_zone[NX * NY * NZ];
    float           mix_vf[NX * NY * NZ];

    DBoptlist      *optlist = NULL;

    int             i, j, k;

    float           xave, yave;
    float           xcenter, ycenter;

    float           theta, dtheta;
    float           r, dr;
    float           h, dh;
    float           dist;

    int             block;
    int             delta_x, delta_y, delta_z;
    int             base_x, base_y, base_z;
    int             n_x, n_y, n_z;

    float           x2[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           y2[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           z2[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           d2[NX * NY * NZ], p2[NX * NY * NZ];
    float           u2[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           v2[(NX + 1) * (NY + 1) * (NZ + 1)];
    float           w2[(NX + 1) * (NY + 1) * (NZ + 1)];
    int             matlist2[NX * NY * NZ];

    /* 
     * Create the mesh.
     */
    meshname = "mesh1";
    coordnames[0] = "xcoords";
    coordnames[1] = "ycoords";
    coordnames[2] = "zcoords";
    coords[0] = x;
    coords[1] = y;
    coords[2] = z;

    ndims = 3;
    dims[0] = NX + 1;
    dims[1] = NY + 1;
    dims[2] = NZ + 1;

    dtheta = -(180. / NX) * (3.1415926 / 180.);
    dh = 1;
    dr = 3. / NY;
    theta = 3.1415926536;

    for (i = 0; i < NX + 1; i++)
    {
        r = 2.;
        for (j = 0; j < NY + 1; j++)
        {
            h = 0.;
            for (k = 0; k < NZ + 1; k++)
            {
                x[k * (NX + 1) * (NY + 1) + ((j * (NX + 1)) + i)] = r * cos(theta);
                y[k * (NX + 1) * (NY + 1) + ((j * (NX + 1)) + i)] = r * sin(theta);
                z[k * (NX + 1) * (NY + 1) + ((j * (NX + 1)) + i)] = h;
                h += dh;
            }
            r += dr;
        }
        theta += dtheta;
    }

    /* 
     * Create the density and pressure arrays.
     */
    var1name = "d";
    var2name = "p";
    xcenter = 0.;
    ycenter = 0.;
    zdims[0] = NX;
    zdims[1] = NY;
    zdims[2] = NZ;

    for (i = 0; i < NX; i++)
    {
        for (j = 0; j < NY; j++)
        {
            for (k = 0; k < NZ; k++)
            {

                xave = (x[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] +
                      x[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i + 1] +
                x[k * (NX + 1) * (NY + 1) + (j + 1) * (NX + 1) + i + 1] +
                x[k * (NX + 1) * (NY + 1) + (j + 1) * (NX + 1) + i]) / 4.;

                yave = (y[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] +
                      y[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i + 1] +
                y[k * (NX + 1) * (NY + 1) + (j + 1) * (NX + 1) + i + 1] +
                y[k * (NX + 1) * (NY + 1) + (j + 1) * (NX + 1) + i]) / 4.;

                dist = sqrt((xave - xcenter) * (xave - xcenter) +
                            (yave - ycenter) * (yave - ycenter));
                d[k * (NX) * (NY) + j * (NX) + i] = dist;
                p[k * (NX) * (NY) + j * (NX) + i] = 1. / (dist + .0001);
            }
        }
    }

    /* 
     *      Create the velocity component arrays.
     */
    var3name = "u";
    var4name = "v";
    var5name = "w";
    xcenter = 0.;
    ycenter = 0.;

    for (i = 0; i < NX + 1; i++)
    {
        for (j = 0; j < NY + 1; j++)
        {
            for (k = 0; k < NZ + 1; k++)
            {
                dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                            (y[j] - ycenter) * (y[j] - ycenter));
                u[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] = (x[i] - xcenter) / dist;
                v[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] = (y[j] - ycenter) / dist;
                w[k * (NX + 1) * (NY + 1) + j * (NX + 1) + i] = 0.;
            }

        }
    }

    /* Create the material array.  */
    matname = "mat1";
    nmats = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;
    dims2[0] = NX;
    dims2[1] = NY;
    dims2[2] = NZ;

    mixlen = 0;

    /*
     * Put in the material in 3 shells.
     */

    for (i = 0; i < NX; i++)
    {
        for (k = 0; k < NZ; k++)
        {
            for (j = 0; j < 10; j++)
            {
                matlist[k * NX * NY + j * NX + i] = 1;
            }
            for (j = 10; j < 20; j++)
            {
                matlist[k * NX * NY + j * NX + i] = 2;
            }
            for (j = 20; j < NY; j++)
            {
                matlist[k * NX * NY + j * NX + i] = 3;
            }
        }

    }

    /* 
     * Now extract the data for this block.
     */

    delta_x = NX / nblocks_x;
    delta_y = NY / nblocks_y;
    delta_z = NZ / nblocks_z;

    coords[0] = x2;
    coords[1] = y2;
    coords[2] = z2;
    dims[0] = delta_x + 1;
    dims[1] = delta_y + 1;
    dims[2] = delta_z + 1;
    zdims[0] = delta_x;
    zdims[1] = delta_y;
    zdims[2] = delta_z;
    dims2[0] = delta_x;
    dims2[1] = delta_y;
    dims2[2] = delta_z;

    /* 
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y * nblocks_z; block++)
    {
        fprintf(stdout, "\t%s\n", dirnames[block]);

        /* 
         * Now extract the data for this block.
         */

        base_x = (block % nblocks_x) * delta_x;
        base_y = ((block % (nblocks_x * nblocks_y)) / nblocks_x) * delta_y;
        base_z = (block / (nblocks_x * nblocks_y)) * delta_z;

        for (k = 0, n_z = base_z; k < delta_z + 1; k++, n_z++)
            for (j = 0, n_y = base_y; j < delta_y + 1; j++, n_y++)
                for (i = 0, n_x = base_x; i < delta_x + 1; i++, n_x++)
                {
                    x2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        x[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    y2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        y[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    z2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        z[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    u2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        u[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    v2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        v[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                    w2[k * (delta_x + 1) * (delta_y + 1) + j * (delta_x + 1) + i] =
                        w[n_z * (NX + 1) * (NY + 1) + n_y * (NX + 1) + n_x];
                }

        for (k = 0, n_z = base_z; k < delta_z; k++, n_z++)
            for (j = 0, n_y = base_y; j < delta_y; j++, n_y++)
                for (i = 0, n_x = base_x; i < delta_x; i++, n_x++)
                {
                    d2[k * delta_x * delta_y + j * delta_x + i] =
                        d[n_z * NX * NY + n_y * NX + n_x];
                    p2[k * delta_x * delta_y + j * delta_x + i] =
                        p[n_z * NX * NY + n_y * NX + n_x];
                    matlist2[k * delta_x * delta_y + j * delta_x + i] =
                        matlist[n_z * NX * NY + n_y * NX + n_x];
                }

        if (DBSetDir(dbfile, dirnames[block]) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n",
                    dirnames[block]);
            return;
        }                              /* if */
        /* Write out the variables. */
        cycle = 48;
        time = 4.8;
        dtime = 4.8;

        optlist = DBMakeOptlist(10);
        DBAddOption(optlist, DBOPT_CYCLE, &cycle);
        DBAddOption(optlist, DBOPT_TIME, &time);
        DBAddOption(optlist, DBOPT_DTIME, &dtime);
        DBAddOption(optlist, DBOPT_XLABEL, "X Axis");
        DBAddOption(optlist, DBOPT_YLABEL, "Y Axis");
        DBAddOption(optlist, DBOPT_ZLABEL, "Z Axis");
        DBAddOption(optlist, DBOPT_XUNITS, "cm");
        DBAddOption(optlist, DBOPT_YUNITS, "cm");
        DBAddOption(optlist, DBOPT_ZUNITS, "cm");

        put_extents(x2,dims[0]*dims[1]*dims[2],varextents[0],block);
        put_extents(y2,dims[0]*dims[1]*dims[2],varextents[1],block);
        put_extents(z2,dims[0]*dims[1]*dims[2],varextents[2],block);
        has_external_zones[block] = 1;
        zonecounts[block] = (dims[0]-1)*(dims[1]-1)*(dims[2]-1);
        DBPutQuadmesh(dbfile, meshname, coordnames, coords,
                      dims, ndims, DB_FLOAT, DB_NONCOLLINEAR,
                      optlist);

        put_extents(d2,(dims[0]-1)*(dims[1]-1)*(dims[2]-1),varextents[3],block);
        DBPutQuadvar1(dbfile, var1name, meshname, d2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(p2,(dims[0]-1)*(dims[1]-1)*(dims[2]-1),varextents[4],block);
        DBPutQuadvar1(dbfile, var2name, meshname, p2, zdims, ndims,
                      NULL, 0, DB_FLOAT, DB_ZONECENT, optlist);

        put_extents(u2,dims[0]*dims[1]*dims[2],varextents[5],block);
        DBPutQuadvar1(dbfile, var3name, meshname, u2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(v2,dims[0]*dims[1]*dims[2],varextents[6],block);
        DBPutQuadvar1(dbfile, var4name, meshname, v2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        put_extents(w2,dims[0]*dims[1]*dims[2],varextents[7],block);
        DBPutQuadvar1(dbfile, var5name, meshname, w2, dims, ndims,
                      NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        matcounts[block] = count_mats((dims[0]-1)*(dims[1]-1)*(dims[2]-1),matlist2,matlists[block]);
        mixlens[block] = mixlen;
        DBPutMaterial(dbfile, matname, meshname, nmats, matnos,
                      matlist2, dims2, ndims, mix_next, mix_mat, mix_zone,
                      mix_vf, mixlen, DB_FLOAT, optlist);

        DBFreeOptlist(optlist);

        if (DBSetDir(dbfile, "..") == -1)
        {
            fprintf(stderr, "Could not return to base directory\n");
            return;
        }                              /* if */
    }                                  /* for */

}                                      /* build_block_curv3d */
