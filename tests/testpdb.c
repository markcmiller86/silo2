#include <score.h>
#include <pdb.h>

char *safe_strdup(const char *);

typedef struct {
    char          *name;
    char          *type;        /* Type of group/object */
    char         **comp_names;  /* Array of component names */
    char         **pdb_names;   /* Array of internal (PDB) variable names */
    int            ncomponents; /* Number of components */
} Group;

void CreateFile (char *filename, char *name, char *type, int num,
     char **comp_names, char **pdb_names);
void ReadFile (char *filename, char *name);

char *comp_names[] = {"coord0",
                      "coord1",
                      "coord2",
                      "min_extents",
                      "max_extents",
                      "facelist",
                      "zonelist",
                      "ndims",
                      "nnodes",
                      "nzones",
                      "facetype",
                      "cycle",
                      "coord_sys",
                      "planar",
                      "origin",
                      "datatype",
                      "time"};
char *pdb_names[]  = {"/mesh_coord0",
                      "/mesh_coord1",
                      "/mesh_coord2",
                      "/mesh_min_extents",
                      "/mesh_max_extents",
                      "'<s>fl'",
                      "'<s>zl'",
                      "'<i>3'",
                      "'<i>1093'",
                      "'<i>1200'",
                      "'<i>100'",
                      "'<i>0'",
                      "'<i>124'",
                      "'<i>124'",
                      "'<i>0'",
                      "'<i>20'",
                      "/time"};

float coord0_data[20] = { 0.,  1.,  2.,  3.,  4.,  5.,  6.,  7.,  8.,  9.,
                         10., 11., 12., 13., 14., 15., 16., 17., 18., 19.};


main ()
{

    CreateFile("abc.pdb", "mesh", "ucdmesh", 17, comp_names, pdb_names);

    ReadFile("abc.pdb", "mesh");
}

void
ReadFile (char *filename, char *name)
{
    int       i;
    PDBfile   *file=NULL;
    Group     *group=NULL;
    char      str[256];
    char      *cptr;
    char      **ccptr;
    char      carray[256];
    float     *fptr;
    float     farray[20];
    float     fval;

    /*
     * Open the file.
     */
    if ((file = lite_PD_open(filename, "r")) == NULL)
    {
        printf("Error opening file.\n");
        exit(-1);
    }

    /*
     * Read some variables and print their results.
     */
    printf("reading group\n");
    lite_PD_read(file, name, &group);
    printf("group->name = %s\n", group->name);
    printf("group->type = %s\n", group->type);
    printf("group->ncomponents = %d\n", group->ncomponents);
    printf("group->comp_names[0] = %s\n", group->comp_names[0]);
    printf("group->pdb_names[0] = %s\n", group->pdb_names[0]);
    for (i = 0; i < 17; i++)
    {
        SFREE(group->comp_names[i]);
        SFREE(group->pdb_names[i]);
    }
    SFREE(group->comp_names);
    SFREE(group->pdb_names);
    SFREE(group->name);
    SFREE(group->type);
    SFREE(group);

    printf("reading group->name\n");
    sprintf(str, "%s->name", name);
    lite_PD_read(file, str, &cptr);
    printf("%s->name = %s\n", name, cptr);
    SFREE(cptr);

    printf("reading group->comp_names\n");
    sprintf(str, "%s->comp_names", name);
    lite_PD_read(file, str, &ccptr);
    printf("%s->comp_names[0] = %s\n", name, ccptr[0]);
    for (i = 0; i < 17; i++)
       SFREE(ccptr[i]);
    SFREE(ccptr);

    printf("reading group->comp_names[1]\n");
    sprintf(str, "%s->comp_names[1]", name);
    lite_PD_read(file, str, &cptr);
    printf("%s->comp_names[1] = %s\n", name, cptr);
    SFREE(cptr);

    printf("reading group->comp_names[1][2:4]\n");
    sprintf(str, "%s->comp_names[1][2:4]", name);
    lite_PD_read(file, str, &carray);
    carray[3] = '\0';
    printf("%s->comp_names[1][2:4] = %s\n", name, carray);

    printf("reading coord0\n");
    lite_PD_read(file, "coord0", farray);
    printf("coord0[4] = %g\n", farray[4]);

    printf("reading coord0(0, 3)\n");
    lite_PD_read(file, "coord0(0, 3)", &fval);
    printf("coord0(0, 3) = %g\n", fval);

    /*
     * Close the file.
     */
    lite_PD_close(file);

    return;
}

void
CreateFile (char *filename, char *name, char *type, int num,
            char **comp_names, char **pdb_names)
{
    int       i;
    int       *null_ptr;
    int       ierr;
    PDBfile   *file=NULL;
    Group     *group=NULL;
    float     *coord0;
    long      ind[6];

    /*
     * Create a file.
     */
    if ((file = lite_PD_create(filename)) == NULL)
    {
        printf("Error creating file.\n");
        exit(-1);
    }

    /*
     * Define the group to PDB.
     */
    null_ptr  = FMAKE(int, "NULL");
    *null_ptr = 0;

    if (lite_PD_defstr(file, "Group",
                       "char    *name",
                       "char    *type",
                       "char    **comp_names",
                       "char    **pdb_names",
                       "integer ncomponents",
                       null_ptr) == NULL)
    {
        printf("Error defining Group structure.\n");
        exit(-1);
    }

    SFREE(null_ptr);

    /*
     * Allocate the group structure and populate it.
     */
    group = MAKE_N(Group, 1);
    group->comp_names = MAKE_N(char *, num);
    group->pdb_names = MAKE_N(char *, num);
    for (i = 0; i < num; i++) {
        group->comp_names[i] = safe_strdup(comp_names[i]);
        group->pdb_names[i] = safe_strdup(pdb_names[i]);
    }
    group->type = safe_strdup(type);
    group->name = safe_strdup(name);
    group->ncomponents = num;

    /*
     * Write the group.
     */
    if (lite_PD_write(file, name, "Group *", &group) == 0)
    {
        printf("Error writing group.\n");
        exit(-1);
    }

    /*
     * Free the group structure.
     */
    for (i = 0; i < group->ncomponents; i++)
    {
        SFREE(group->comp_names[i]);
        SFREE(group->pdb_names[i]);
    }
    SFREE(group->comp_names);
    SFREE(group->pdb_names);
    SFREE(group->name);
    SFREE(group->type);
    SFREE(group);

    /*
     * Write an array.
     */
    coord0 = MAKE_N(float, 20);
    for (i = 0; i < 20; i++) coord0[i] = coord0_data[i];
    ind[0] = 0;
    ind[1] = 1;
    ind[2] = 1;
    ind[3] = 0;
    ind[4] = 9;
    ind[5] = 1;
    if (lite_PD_write_alt(file, "coord0", "float", coord0, 2, ind) == 0)
    {
        printf("Error writing array.\n");
        exit(-1);
    }

    SFREE(coord0);

    /*
     * Close the file.
     */
    lite_PD_close(file);

    return;
}