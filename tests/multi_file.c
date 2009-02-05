/*
 * Create a multi-file multi-block mesh.
 *
 */

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <silo.h>

#define MAXBLOCKS       400       /* Maximum number of blocks in an object */
#define STRLEN          60
#define MIXMAX          20000     /* Maximum length of the mixed arrays */
#define NFILES          8         /* The number of files to create */

#define MIN(x, y) (x) < (y) ? (x) : (y)
#define MAX(x, y) (x) > (y) ? (x) : (y)

#define ALLOC_N(T,N)    ((T*)calloc((N),sizeof(T)))
#define FREE(M)         if(M){free(M);(M)=NULL;}

void fill_bkgr(int *, int, int, int, int);
void fill_mat(float *, float *, float *, int *, int, int, int,
    int *, int *, int *, float *, int *, int, double, double, double, double);

int  build_multi(char *, int, char *, int, int, int);

void build_block_ucd3d(char *, int, char *, int, int, int);

/***********************************************************************
 *
 * Purpose:
 *    Generate a multi file 3d ucd multi block test file.
 *
 * Programmer:  Eric Brugger
 * Date:        September 17, 2001
 *
 ***********************************************************************/
int
main(int argc, char *argv[])
{
    int            i;
    char          *basename="ucd3d";
    char          *file_ext="pdb";
    int            driver=DB_PDB;

    /*
     * Parse the command-line.
     */
    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "DB_PDB"))
        {
            driver = DB_PDB;
            file_ext = ".pdb";
        }
        else if (!strcmp(argv[i], "DB_HDF5"))
        {
            driver = DB_HDF5;
            file_ext = ".h5";
        }
        else
        {
            fprintf(stderr, "%s: ignored argument `%s'\n", argv[0], argv[i]);
        }
    }

    /* 
     * Create the multi-block ucd 3d mesh.
     */
    build_multi(basename, driver, file_ext, 6, 8, 6);

    return 0;
}

/***********************************************************************      
 *
 * Purpose:
 *    Fill the entire material array with the material "matno".
 *
 * Programmer:  Eric Brugger
 * Date:        September 17, 2001
 *
 ***********************************************************************/
void
fill_bkgr(int *matlist, int nx, int ny, int nz, int matno)
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

/***********************************************************************      
 *
 * Purpose:
 *    Fill the specified material array with sphere centered at
 *    "xcenter", "ycenter", "zcenter" and radius of "radius" with
 *    the material "matno".
 *
 * Programmer:  Eric Brugger
 * Date:        September 17, 2001
 *
 ***********************************************************************/
void
fill_mat(float *x, float *y, float *z, int *matlist, int nx,
         int ny, int nz, int *mix_next, int *mix_mat, int *mix_zone,
         float *mix_vf, int *mixlen, int matno, double xcenter,
         double ycenter, double zcenter, double radius)
{
    int             i, j, k, l, m, n;
    double          dist;
    int             cnt;
    int             mixlen2;
    int            *itemp=NULL;
    float           dx, dy, dz;
    float           xx[10], yy[10], zz[10];

    mixlen2 = *mixlen;

    itemp = ALLOC_N(int, (nx + 1) * (ny + 1) * (nz + 1));

    for (i = 0; i < nx; i++)
    {
        for (j = 0; j < ny; j++)
        {
            for (k = 0; k < nz; k++)
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
                cnt = itemp[(i)   * (nx+1) * (ny+1) + (j)   * (nx+1) + k  ] +
                      itemp[(i)   * (nx+1) * (ny+1) + (j+1) * (nx+1) + k  ] +
                      itemp[(i)   * (nx+1) * (ny+1) + (j+1) * (nx+1) + k+1] +
                      itemp[(i)   * (nx+1) * (ny+1) + (j)   * (nx+1) + k+1] +
                      itemp[(i+1) * (nx+1) * (ny+1) + (j)   * (nx+1) + k  ] +
                      itemp[(i+1) * (nx+1) * (ny+1) + (j+1) * (nx+1) + k  ] +
                      itemp[(i+1) * (nx+1) * (ny+1) + (j+1) * (nx+1) + k+1] +
                      itemp[(i+1) * (nx+1) * (ny+1) + (j)   * (nx+1) + k+1];
                if (cnt == 0)
                {
                    /* EMPTY */
                }
                else if (cnt == 8)
                {
                    matlist[i * nx * ny + j * nx + k] = matno;
                }
                else
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
                    matlist[i * nx * ny + j * nx + k] = -(mixlen2 + 1);
                    mix_mat[mixlen2] = matno - 1;
                    mix_mat[mixlen2 + 1] = matno;
                    mix_next[mixlen2] = mixlen2 + 2;
                    mix_next[mixlen2 + 1] = 0;
                    mix_zone[mixlen2] = i * nx * ny + j * nx + k;
                    mix_zone[mixlen2 + 1] = i * nx * ny + j * nx + k;
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

/***********************************************************************      
 *
 * Purpose:
 *    Make a multi-block mesh, multi-block variables, and a multi-block
 *    material based on the given meshtype, dimensionality of the mesh,
 *    and number of blocks in the x-direction, y-direction, and
 *    z-direction.  Also specify if the mesh is collinear when creating
 *    a quad mesh.  The total number of blocks created for the mesh
 *    equals number of blocks in x-direction times number of blocks in
 *    y-direction times number of blocks in z-direction.
 *
 *    nblocks = nblocks_x * nblocks_y * nblocks_z
 *
 * Programmer:  Eric Brugger
 * Date:        September 17, 2001
 *
 ***********************************************************************/
int
build_multi(char *basename, int driver, char *file_ext,
            int nblocks_x, int nblocks_y, int nblocks_z)
{
    int             i;
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

    DBoptlist      *optlist=NULL;

    char            filename[80];
    DBfile         *dbfile=NULL;

    int             nblocks = nblocks_x * nblocks_y * nblocks_z;

    /* 
     * Initialize the names and create the directories for the blocks.
     */
    for (i = 0; i < nblocks; i++)
    {
        int       filenum;
        char      prefix[80];

        filenum = i / (nblocks / NFILES);
        sprintf(prefix, "%s%d.%s:/block%d/", basename, filenum, file_ext, i);

        sprintf(names[i], "%smesh1", prefix);
        meshnames[i] = names[i];
        meshtypes[i] = DB_UCDMESH;

        sprintf(names1[i], "%sd", prefix);
        sprintf(names2[i], "%sp", prefix);
        sprintf(names3[i], "%su", prefix);
        sprintf(names4[i], "%sv", prefix);
        sprintf(names5[i], "%sw", prefix);
        var1names[i] = names1[i];
        var2names[i] = names2[i];
        var3names[i] = names3[i];
        var4names[i] = names4[i];
        var5names[i] = names5[i];
        vartypes[i] = DB_UCDVAR;

        sprintf(names0[i], "%smat1", prefix);
        matnames[i] = names0[i];
    }

    /*
     * Create the blocks.
     */
    build_block_ucd3d(basename, driver, file_ext, 
                      nblocks_x, nblocks_y, nblocks_z);

    /*
     * Create the file.
     */
    sprintf(filename, "%s_root.%s", basename, file_ext);
    if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
        "multi-file ucd 3d test file", driver)) == NULL)
    {
        fprintf(stderr, "Could not create '%s'.\n", filename);
        return -1;
    }

    /*
     * Create the option lists for the multi-block calls.
     */
    cycle = 48;
    time = 4.8;
    dtime = 4.8;
    nmatnos = 3;
    matnos[0] = 1;
    matnos[1] = 2;
    matnos[2] = 3;

    optlist = DBMakeOptlist(5);
    DBAddOption(optlist, DBOPT_CYCLE, &cycle);
    DBAddOption(optlist, DBOPT_TIME, &time);
    DBAddOption(optlist, DBOPT_DTIME, &dtime);
    DBAddOption(optlist, DBOPT_NMATNOS, &nmatnos);
    DBAddOption(optlist, DBOPT_MATNOS, matnos);

    /*
     * Create the multi-block mesh, variables and material.
     */
    if (DBPutMultimesh(dbfile, "mesh1", nblocks,
                       meshnames, meshtypes, optlist) == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi mesh\n");
        return -1;
    }
    if (DBPutMultivar(dbfile, "d", nblocks, var1names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var d\n");
        return -1;
    }
    if (DBPutMultivar(dbfile, "p", nblocks, var2names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var p\n");
        return -1;
    }
    if (DBPutMultivar(dbfile, "u", nblocks, var3names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var u\n");
        return -1;
    }
    if (DBPutMultivar(dbfile, "v", nblocks, var4names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var v\n");
        return -1;
    }
    if (DBPutMultivar(dbfile, "w", nblocks, var5names, vartypes, optlist)
        == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi var w\n");
        return -1;
    }
    if (DBPutMultimat(dbfile, "mat1", nblocks, matnames, optlist) == -1)
    {
        DBFreeOptlist(optlist);
        fprintf(stderr, "Error creating multi material\n");
        return -1;
    }
    DBFreeOptlist(optlist);

    /*
     * Close the file.
     */
    DBClose(dbfile);

    return 0;
}

/***********************************************************************
 * Purpose:
 *    Build a 3-d UCD mesh and add it to the open database.
 *
 * Programmer:  Eric Brugger
 * Date:        September 17, 2001
 *
 ***********************************************************************/
void
build_block_ucd3d(char *basename, int driver, char *file_ext,
                  int nblocks_x, int nblocks_y, int nblocks_z)
{
    int             nx, ny, nz;
    int             cycle;
    float           time;
    double          dtime;
    char           *coordnames[3];
    float          *coords[3];
    float          *x=NULL, *y=NULL, *z=NULL;

    char           *meshname=NULL, *var1name=NULL, *var2name=NULL,
                   *var3name=NULL, *var4name=NULL, *var5name=NULL;
    char           *matname=NULL;
    float          *vars[1];
    char           *varnames[1];
    float          *d=NULL, *p=NULL, *u=NULL, *v=NULL, *w=NULL;

    int             nmats;
    int             matnos[3];
    int            *matlist=NULL;
    int             mixlen;
    int            *mix_next=NULL, *mix_mat=NULL, *mix_zone=NULL;
    float          *mix_vf=NULL;
    float          *xstrip=NULL, *ystrip=NULL, *zstrip=NULL;

    int             i, j, k;
    int             iz;
    float           xcenter, ycenter;
    float           theta, dtheta;
    float           r, dr;
    float           h, dh;
    float           dist;

    int             block;
    int             delta_x, delta_y, delta_z;

    nx = 120;
    ny = 160;
    nz = 120;

    x = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));
    y = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));
    z = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));

    d = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));
    p = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));
    u = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));
    v = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));
    w = ALLOC_N(float, (nx+1)*(ny+1)*(nz+1));

    matlist  = ALLOC_N(int, nx*ny*nz);
    mix_next = ALLOC_N(int, 85000);
    mix_mat  = ALLOC_N(int, 85000);
    mix_zone = ALLOC_N(int, 85000);
    mix_vf   = ALLOC_N(float, 85000);

    xstrip   = ALLOC_N(float, nx+ny+nz);
    ystrip   = ALLOC_N(float, nx+ny+nz);
    zstrip   = ALLOC_N(float, nx+ny+nz);

    /* 
     * Create the coordinate arrays for the entire mesh.
     */
    dh = 20. / (float)nx;
    dtheta = (180. / (float)ny) * (3.1415926 / 180.);
    dr = 3. / (float)nz;
    h = 0.;
    for (i = 0; i < nx + 1; i++)
    {
        theta = 0.;
        for (j = 0; j < ny + 1; j++)
        {
            r = 2.;
            for (k = 0; k < nz + 1; k++)
            {
                x[i * (nx+1)*(ny+1) + j * (nx+1) + k] = r * cos(theta);
                y[i * (nx+1)*(ny+1) + j * (nx+1) + k] = r * sin(theta);
                z[i * (nx+1)*(ny+1) + j * (nx+1) + k] = h;
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
    for (i = 0; i < nx + 1; i++)
    {
        for (j = 0; j < ny + 1; j++)
        {
            for (k = 0; k < nz + 1; k++)
            {
                dist = sqrt((x[i * (nx+1)*(ny+1) + j * (nx+1) + k] - xcenter) *
                            (x[i * (nx+1)*(ny+1) + j * (nx+1) + k] - xcenter) +
                            (y[i * (nx+1)*(ny+1) + j * (nx+1) + k] - ycenter) *
                            (y[i * (nx+1)*(ny+1) + j * (nx+1) + k] - ycenter));
                d[i * (nx+1)*(ny+1) + j * (nx+1) + k] = dist;
                p[i * (nx+1)*(ny+1) + j * (nx+1) + k] = 1. / (dist + .0001);
            }
        }
    }

    /* 
     * Create the velocity component arrays for the entire mesh.
     */
    xcenter = 0.;
    ycenter = 0.;
    for (i = 0; i < nx + 1; i++)
    {
        for (j = 0; j < ny + 1; j++)
        {
            for (k = 0; k < nz + 1; k++)
            {
                dist = sqrt((x[i] - xcenter) * (x[i] - xcenter) +
                            (y[j] - ycenter) * (y[j] - ycenter));
                u[i * (nx+1)*(ny+1) + j * (nx+1) + k] = (x[i] - xcenter) / dist;
                v[i * (nx+1)*(ny+1) + j * (nx+1) + k] = (y[j] - ycenter) / dist;
                w[i * (nx+1)*(ny+1) + j * (nx+1) + k] = 0.;
            }
        }
    }

    /* 
     * Put in the material for the entire mesh.
     */
    fill_bkgr(matlist, nx, ny, nz, 1);

    for (i = 0; i < ny; i++)
    {
        xstrip[i] = (float)i;
        ystrip[i] = (float)i;
        zstrip[i] = (float)i;
    }

    mixlen = 0;
    fill_mat(xstrip, ystrip, zstrip, matlist, nx, ny, nz, mix_next,
             mix_mat, mix_zone, mix_vf, &mixlen, 2, 60., 80., 60., 40.);
    fill_mat(xstrip, ystrip, zstrip, matlist, nx, ny, nz, mix_next,
             mix_mat, mix_zone, mix_vf, &mixlen, 3, 60., 80., 60., 20.);
    if (mixlen > 85000)
    {
        printf("memory overwrite: mixlen = %d > 85000\n", mixlen);
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
    delta_x = nx / nblocks_x;
    delta_y = ny / nblocks_y;
    delta_z = nz / nblocks_z;

    /* 
     * Create the blocks for the multi-block object.
     */

    for (block = 0; block < nblocks_x * nblocks_y * nblocks_z; block++)
    {
        char            dirname[80];
        int             filenum;
        char            filename[80];

        int             imin, imax, jmin, jmax, kmin, kmax;
        int             nnx, nny, nnz;

        float          *x2=NULL, *y2=NULL, *z2=NULL;
        float          *d2=NULL, *p2=NULL, *u2=NULL, *v2=NULL, *w2=NULL;
        int            *matlist2=NULL, *ghost=NULL;

        int             n_x, n_y, n_z;

        int             nreal;
        int             ighost;
        int             itemp;
        int             hi_off;

        int             one = 1;

        int             nfaces, nzones, nnodes;
        int             lfacelist, lzonelist;
        int             fshapesize, fshapecnt;
        int             zshapetype, zshapesize, zshapecnt;
        int            *zonelist=NULL;
        int            *facelist=NULL;
        int            *zoneno=NULL;

        DBfacelist     *fl=NULL;

        DBoptlist      *optlist=NULL;
        DBobject       *obj=NULL;
        DBfile         *dbfile;

        sprintf(dirname, "/block%d", block);
        filenum = block / ((nblocks_x * nblocks_y * nblocks_z) / NFILES);
        sprintf(filename, "%s%d.%s", basename, filenum, file_ext);

        fprintf(stderr, "\t%s:%s\n", filename, dirname);

        /* 
         * Now extract the data for this block.
         */
        imin = (block % nblocks_x) * delta_x - 1;
        imax = MIN(imin + delta_x + 3, nx + 1);
        imin = MAX(imin, 0);
        nnx = imax - imin;
        jmin = ((block % (nblocks_x * nblocks_y)) / nblocks_x) * delta_y - 1;
        jmax = MIN(jmin + delta_y + 3, ny + 1);
        jmin = MAX(jmin, 0);
        nny = jmax - jmin;
        kmin = (block / (nblocks_x * nblocks_y)) * delta_z - 1;
        kmax = MIN(kmin + delta_z + 3, nz + 1);
        kmin = MAX(kmin, 0);
        nnz = kmax - kmin;

        x2 = ALLOC_N(float, nnx*nny*nnz);
        y2 = ALLOC_N(float, nnx*nny*nnz);
        z2 = ALLOC_N(float, nnx*nny*nnz);
        d2 = ALLOC_N(float, nnx*nny*nnz);
        p2 = ALLOC_N(float, nnx*nny*nnz);
        u2 = ALLOC_N(float, nnx*nny*nnz);
        v2 = ALLOC_N(float, nnx*nny*nnz);
        w2 = ALLOC_N(float, nnx*nny*nnz);
        matlist2 = ALLOC_N(int, (nnx-1)*(nny-1)*(nnz-1));
        ghost    = ALLOC_N(int, (nnx-1)*(nny-1)*(nnz-1));

        coords[0] = x2;
        coords[1] = y2;
        coords[2] = z2;

        for (k = 0, n_z = kmin; n_z < kmax; k++, n_z++)
            for (j = 0, n_y = jmin; n_y < jmax; j++, n_y++)
                for (i = 0, n_x = imin; n_x < imax; i++, n_x++)
                {
                    x2[k * nnx * nny + j * nnx + i] =
                        x[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    y2[k * nnx * nny + j * nnx + i] =
                        y[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    z2[k * nnx * nny + j * nnx + i] =
                        z[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    d2[k * nnx * nny + j * nnx + i] =
                        d[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    p2[k * nnx * nny + j * nnx + i] =
                        p[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    u2[k * nnx * nny + j * nnx + i] =
                        u[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    v2[k * nnx * nny + j * nnx + i] =
                        v[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                    w2[k * nnx * nny + j * nnx + i] =
                        w[n_z * (nx + 1) * (ny + 1) + n_y * (nx + 1) + n_x];
                }

        zonelist = ALLOC_N(int, (nnx-1)*(nny-1)*(nnz-1)*8);

        iz = 0;
        for (k = 0, n_z = kmin; n_z < kmax - 1; k++, n_z++)
            for (j = 0, n_y = jmin; n_y < jmax - 1; j++, n_y++)
                for (i = 0, n_x = imin; n_x < imax - 1; i++, n_x++)
                {
                    zonelist[iz]     = (k+0) * nnx*nny + (j+1) * nnx + i+1;
                    zonelist[iz + 1] = (k+0) * nnx*nny + (j+0) * nnx + i+1;
                    zonelist[iz + 2] = (k+1) * nnx*nny + (j+0) * nnx + i+1;
                    zonelist[iz + 3] = (k+1) * nnx*nny + (j+1) * nnx + i+1;
                    zonelist[iz + 4] = (k+0) * nnx*nny + (j+1) * nnx + i+0;
                    zonelist[iz + 5] = (k+0) * nnx*nny + (j+0) * nnx + i+0;
                    zonelist[iz + 6] = (k+1) * nnx*nny + (j+0) * nnx + i+0;
                    zonelist[iz + 7] = (k+1) * nnx*nny + (j+1) * nnx + i+0;
                    iz += 8;

                    matlist2[k * (nnx - 1) * (nny - 1) + j * (nnx - 1) + i] =
                        matlist[n_z * nx * ny + n_y * nx + n_x];

                    if (((k == 0 || n_z == kmax - 2) &&
                         (n_z != 0 && n_z != nz - 1)) ||
                        ((j == 0 || n_y == jmax - 2) &&
                         (n_y != 0 && n_y != ny - 1)) ||
                        ((i == 0 || n_x == imax - 2) &&
                         (n_x != 0 && n_x != nx - 1)))
                        ghost[k * (nnx-1) * (nny-1) + j * (nnx-1) + i] = 1;
                    else
                        ghost[k * (nnx-1) * (nny-1) + j * (nnx-1) + i] = 0;
                }

        /* 
         * Resort the zonelist, matlist so that the ghost zones are at the
         * end.
         */
        nzones = (nnx - 1) * (nny - 1) * (nnz - 1);
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
        nnodes = nnx * nny * nnz;
        hi_off = nzones - nreal;

        zshapesize = 8;
        zshapecnt = nzones;
        zshapetype = DB_ZONETYPE_HEX;
        lzonelist = nzones * 8;

        fl = DBCalcExternalFacelist2(zonelist, nnodes, 0, hi_off, 0,
                                     &zshapetype, &zshapesize, &zshapecnt, 1,
                                     matlist2, 0);

        facelist = ALLOC_N(int, fl->lnodelist);
        zoneno   = ALLOC_N(int, fl->nfaces);

        nfaces = fl->nfaces;
        fshapecnt = fl->nfaces;
        fshapesize = 4;
        lfacelist = fl->lnodelist;
        for (i = 0; i < lfacelist; i++)
            facelist[i] = fl->nodelist[i];
        for (i = 0; i < nfaces; i++)
            zoneno[i] = fl->zoneno[i];

        DBFreeFacelist(fl);

        /*
         * If the block number is devisible by 1/NFILES of the total number
         * of blocks then create the file, otherwise just open the file
         * since it has already been created.
         */
        if (block % ((nblocks_x * nblocks_y * nblocks_z) / NFILES) == 0)
        {
            if ((dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                "multi-file ucd 3d test file", driver)) == NULL)
            {
                fprintf(stderr, "Could not create '%s'.\n", filename);
                return;
            }
        }
        else
        {
            if ((dbfile = DBOpen(filename, driver, DB_APPEND)) == NULL)
            {
                fprintf(stderr, "Could not open '%s'.\n", filename);
                return;
            }
        }

        /*
         * Make the directory for the block and cd into it.
         */
        if (DBMkDir(dbfile, dirname) == -1)
        {
            fprintf(stderr, "Could not make directory \"%s\"\n", dirname);
            return;
        }
        if (DBSetDir(dbfile, dirname) == -1)
        {
            fprintf(stderr, "Could not set directory \"%s\"\n", dirname);
            return;
        }

        /*
         * Write out the mesh and variables.
         */
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
        DBAddOption(optlist, DBOPT_HI_OFFSET, &hi_off);

        if (nfaces > 0)
            DBPutFacelist(dbfile, "fl1", nfaces, 3, facelist, lfacelist, 0,
                      zoneno, &fshapesize, &fshapecnt, 1, NULL, NULL, 0);

        /* 
         * Output the zonelist.  This is being done at the object
         * level to add the hi_offset option which can't be output
         * with the DBPutZonelist routine.
         */
        obj = DBMakeObject("zl1", DB_ZONELIST, 10);

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
        if (nfaces > 0)
            DBPutUcdmesh(dbfile, meshname, 3, coordnames, coords,
                         nnodes, nzones, "zl1", "fl1", DB_FLOAT, optlist);
        else
            DBPutUcdmesh(dbfile, meshname, 3, coordnames, coords,
                         nnodes, nzones, "zl1", NULL, DB_FLOAT, optlist);

        vars[0] = d2;
        varnames[0] = var1name;
        DBPutUcdvar(dbfile, var1name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        vars[0] = p2;
        varnames[0] = var2name;
        DBPutUcdvar(dbfile, var2name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        vars[0] = u2;
        varnames[0] = var3name;
        DBPutUcdvar(dbfile, var3name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        vars[0] = v2;
        varnames[0] = var4name;
        DBPutUcdvar(dbfile, var4name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        vars[0] = w2;
        varnames[0] = var5name;
        DBPutUcdvar(dbfile, var5name, meshname, 1, varnames, vars,
                    nnodes, NULL, 0, DB_FLOAT, DB_NODECENT, optlist);

        DBPutMaterial(dbfile, matname, meshname, nmats, matnos,
                      matlist2, &nzones, 1, mix_next, mix_mat, mix_zone,
                      mix_vf, mixlen, DB_FLOAT, optlist);

        DBFreeOptlist(optlist);

        FREE(x2);
        FREE(y2);
        FREE(z2);
        FREE(d2);
        FREE(p2);
        FREE(u2);
        FREE(v2);
        FREE(w2);
        FREE(matlist2);
        FREE(ghost);

        FREE(zonelist);
        FREE(facelist);
        FREE(zoneno);

        /*
         * Close the file.
         */
        DBClose(dbfile);
    }

    FREE(x);
    FREE(y);
    FREE(z);

    FREE(d);
    FREE(p);
    FREE(u);
    FREE(v);
    FREE(w);

    FREE(matlist);
    FREE(mix_next);
    FREE(mix_mat);
    FREE(mix_zone);
    FREE(mix_vf);

    FREE(xstrip);
    FREE(ystrip);
    FREE(zstrip);
}
