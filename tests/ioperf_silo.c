#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <silo.h>
#include <std.c>

#include <ioperf.h>

/*
 * Implement ioperf's I/O interface using Silo functions
 */

static options_t options;
static const char *filename;
DBfile *dbfile;
static int has_mesh = 0;
static int driver = DB_HDF5;

static int Open_silo(ioflags_t iopflags)
{
    dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL, "ioperf test file", driver);
    if (!dbfile) return 0;
    return 1;
}

static int Write_silo(void *buf, size_t nbytes)
{
    static int n = 0;
    int dims[3] = {1, 1, 1};
    int status;

    dims[0] = nbytes / sizeof(double);
    if (!has_mesh)
    {
        char *coordnames[] = {"x"};
        void *coords[3] = {0, 0, 0};
        coords[0] = buf;
        has_mesh = 1;
        status = DBPutQuadmesh(dbfile, "mesh", coordnames, coords, dims, 1, DB_DOUBLE, DB_COLLINEAR, 0);
    }
    else
    {
        char dsname[64];
        sprintf(dsname, "data_%07d", n++);
        status = DBPutQuadvar1(dbfile, dsname, "mesh", buf, dims, 1, 0, 0, DB_DOUBLE, DB_NODECENT, 0);
    }

    if (status < 0) return 0;
    return nbytes;
}

static int Read_silo(void *buf, size_t nbytes)
{
    char dsname[64];
    static int n = 0;
    void *status;
    sprintf(dsname, "data_%07d", n++);
    status = DBGetQuadvar(dbfile, dsname);
    if (status == 0) return 0;
    return nbytes;
}

static int Close_silo()
{
    CleanupDriverStuff();
    if (DBClose(dbfile) < 0) return 0;
    return 1;
}

static int ProcessArgs_silo(int argi, int argc, char *argv[])
{
    int i,n,nerrors=0;
    DBoptlist *opts = DBMakeOptlist(30);
    for (i=argi; i<argc; i++)
    {
        errno=0;
        if (!strcmp(argv[i], "--driver"))
        {
            i++;
	    driver = StringToDriver(argv[i]);
        }
        else if (!strcmp(argv[i], "--checksums"))
        {
            DBSetEnableChecksums(1);
        }
        else if (!strcmp(argv[i], "--hdf5friendly"))
        {
            DBSetFriendlyHDF5Names(1);
        }
        else if (!strcmp(argv[i], "--compression"))
        {
            char compstr[256];
            i++;
            sprintf(compstr, "METHOD=%s", argv[i]);
            DBSetCompression(compstr);
        }
        else goto fail;
    }
    return 0;

fail:
    fprintf(stderr, "%s: bad argument `%s' (\"%s\")\n", argv[0], argv[i],
        errno?strerror(errno):"");
    return 1;
}

static iointerface_t *CreateInterfaceReal(int argi, int argc, char *argv[], const char *_filename, const options_t *opts)
{
    iointerface_t *retval;

    options = *opts;

    DBShowErrors(DB_ALL, NULL);

    if (ProcessArgs_silo(argi, argc, argv) != 0)
        return 0;

    filename = strdup(_filename);

    retval = (iointerface_t*) calloc(sizeof(iointerface_t),1);
    retval->Open = Open_silo;
    retval->Write = Write_silo;
    retval->Read = Read_silo;
    retval->Close = Close_silo;

    return retval;
}

#ifdef STATIC_PLUGINS
iointerface_t *CreateInterface_silo(int argi, int argc, char *argv[], const char *_filename, const options_t *opts)
{
    return CreateInterfaceReal(argi, argc, argv, _filename, opts);
}
#else
iointerface_t *CreateInterface(int argi, int argc, char *argv[], const char *_filename, const options_t *opts)
{
    return CreateInterfaceReal(argi, argc, argv, _filename, opts);
}
#endif
