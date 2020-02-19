# PPM Tools library documentation
This C library is a collection of tools that aids in the parsing and compression
of High Performance Computing (HPC) Program Prediction Models (PPM). It has been
developed and tested with GCC on Linux. This library is part of the bachelor's thesis
*Compression of the Prediction Models required for Plan Based Scheduling on HPC Nodes*.

## Prerequisites
Apart from the build essentials, this library also requires the [GNU 
scientific library](https://www.gnu.org/software/gsl/) to be installed.

## How to build
Run `make` to create a library archive `libppmtools.a` that can be statically
linked. By default, the library is compiled with `-O3` flags, which hinders
debugging. By defining the complilation flag `OPTF_DEBUG` the library will be 
compiled with `-O0 -g3`:
```shell
$ make OPTF_DEBUG=1
```
There is also an example program available. After the library has been built, 
run make in the `./example` folder to compile the `example` program.

## Including and linking
The header file `./inc/PPM_tools.h` has to be included. The compiled library has
to be statically linked with `-lppmtools`. It is also required to link against
the [GNU scientific library](https://www.gnu.org/software/gsl/) with `-lgsl -lgslcblas -lm`.
The `./example/Makefile` file shows how the example program provided with this
library is built and linked. 
## Usage
There are a couple abstract steps required in order to accomplish model compression:

* Model parsing - read the model from the source file and create the model graph, as a DAG.
* PPM construction - convert the parsed model to a PPM tree
* Aimed discovery - search for similar subgraphs in the tree that provide good similar segment candidates
* Segment bucketing - establish similarity relations between segments and convert them to bucketed segments
* Segment compression - establish equivalence relations between segments and remove duplicates
* Graph compression - reduce the similar subgraphs to one representative
* Compressed model serialization - export the compressed model
as a binary file 

We will go trough the steps and the methods involved by making use of the example program provided with the library.
**For a description of the methods involved, see their
definitions in the source.**

### Model parsing
The model parsing is done within a parsing context object. Each parsing context is associated with a model source file and will hold the model graph as a DAG. 

```c
int retval;
FILE *mod_inputf = fopen(model_input_fname, "r");

if (!mod_inputf) {
    printf("Error opening input file %s\n", model_input_fname);
    return -1;
}

/* Declare and initialize the parsing context. */
MParser parser_ctx;
retval = MParser_init(&parser_ctx, mod_inputf, -1, -1);
if (retval != MP_ok) {
    printf("Model parser: init failed with code %d.\n", retval);
    return -1;
}

/* Parse the model. After completion, the context will hold the PPM graph
 * as a DAG, with the tasks still as vertices. */
retval = MParser_parse(&parser_ctx);
if (retval != MP_ok) {
    printf("Model parser: parsing failed with code %d.\n", retval);
    return -1;
}
printf("Model parser: model input file %s parsed.\n", model_input_fname);

fclose(mod_inputf);
```
### PPM construction
This step will convert the DAG in the parsing context into the PPM tree. A Program Model (PM) context has to be created. 

```c
    /* Create a PPM context. */
    PMContext *pm_ctx = PMContext_create();
    if (!pm_ctx) {
        printf("PM: Cannot create prediction model context.\n");
        return -1;
    }
```
Since the tasks in the DAG will be encapsulated into segments, a segment context is also required to be prepared. The type of segment is `TaskSegRaw`. This is an impmementation of the `TaskSeg` abstract class and holds the weights of the task as they are, uncompressed.
```c

    /* Allocate and initialize a context for the raw task segments. */
    TaskSegRawCtx *tsr_ctx = _obj_alloc(sizeof(TaskSegRawCtx));
    retval = TaskSegRawCtx_init(tsr_ctx, PARAM_MU_MAX, PARAM_SIGMA_MAX);
    if (retval) {
        printf("TaskSeg: cannot init TaskSegRaw context.\n");
        return -1;
    }

    /* Build the PPM tree from the DAG contained in the parser context. The
     * tasks, available as vertices, are encapsulated in raw segments. */
    retval = PMV_build_graph(&parser_ctx, pm_ctx, tsr_ctx);
    if (retval)  {
        printf("PM: cannot build prediction model tree.\n");
        return -1;
    }

    /* We don't need the parser context anymore, as all the operations are from
     * now on done on the PPM tree contained by the PM context ('pm_ctx'). */
    MParser_deinit(&parser_ctx);
```
### Aimed discovery
The aimed discovery performs pattern recognition inside the PPM tree with the scope of delivering similar segment candidates. At this moment, there are three routines implemented. All the found vertices that are roots of similar trees are grouped together.

```c
    /* Aimed minining. This has the scope of finding similar segments
     * candidates. All the vertices found to be roots of similar PPMs are
     * grouped together. */
    GM_mine_for_symm(pm_ctx);
    GM_mine_for_asymm(pm_ctx);
    GM_mine_recurrence(pm_ctx);
```

### Segment bucketing
The segment vertices that are grouped together form candidate pools for similar segments. In this step, each pool is subdivided in segment clusters. All the segments in a cluster are similar to each other. For each cluster, a dictionary for bucketing is created, then all the segments in that cluster are bucketized using the same dictionary.

The first thing to do is therefore to initialize the contexts for bucketized segments (`TaskSegBuck`) and the dictionary (`TCDict`). `TaskSegBuck` is another implementation of the abstract class `TaskSeg`.
```c
    /* Allocate and initialize the context for bucketized segments. */
    TaskSegBuckCtx *tsb_ctx = _obj_alloc(sizeof(TaskSegBuckCtx));
    retval = TaskSegBuckCtx_init(tsb_ctx);
    if (retval) {
        printf("TaskSeg: cannot init TaskSegBuck context.\n");
        return -1;
    }

    /* Allocate and initialize the context for the dictionaries used by the
     * bucketed segments. */
    TCDictCtx *dict_ctx = _obj_alloc(sizeof(TCDictCtx));
    retval = TCDictCtx_init(dict_ctx);
    if (retval) {
        printf("TCDict: cannot init TCDict context.\n");
        return -1;
    }
```
The vertex group class (`PMVG`) is an implementation of the abstract class `Elem`, which tracks its objects relative to a context. The objects are stored in an iterable list. We can now iterate trough all segment vertex groups and create a cluster context (`SegClusterCtx`) for each, then bucketize the segments. After this is done, the `TaskSegRaw` objects in the PPM tree are replaced with `TaskSegBuck` objects.
```c
    /* The segments in segment vertex groups are forming candidates for similar
     * segments. */
    PMVG *gp = PMContext_get_grouplist(pm_ctx);
    while (gp) {
        if (gp->cpmv.type == PMV_seg) {
            /* For each segment vertex group, create clusters of similar
             * segments. */
            SegClusterCtx *cluster_ctx = SegClusterCtx_create(gp, PARAM_K, 0);
            if (!cluster_ctx) {
                printf("Seg Cluster: cannot create cluster.\n");
                return -1;
            }
            /* For each cluster, a dictionary is created. All the segments in
             * the cluster are then bucketized using this dictionary. The raw
             * segments in the PPM tree are then replaces with their bucketized
             * versions.*/
            SegClusterCtx_compress(cluster_ctx, tsr_ctx, tsb_ctx, dict_ctx);
            SegClusterCtx_destroy(cluster_ctx);
        }
        gp = (PMVG*)((Elem*)gp)->next_p;
    }
```
The abstract class `TaskSeg` is also an implementation of the `Elem` class, which means we can also track all types of segments. Because the raw segments are not needed anymore (all the references in the PPM tree were replaced), we can discard them by de-initializing their context.

```c
    /* We no longer need the raw segments, as the tree contains bucketized
     * segments now. */
    Object_deinit((Object*)tsr_ctx);
    _obj_free(tsr_ctx);
```
### Segment compression
After the segments have been bucketized, some of them may be equivalent. This step is similar to the one above: clusters are formed again, this time the segments in a cluster being equivalent to each other. For each cluster, only one representative needs to be stored.
```c
    /* The segments in a segment vertex group are now forming candidates for
     * equivalent segments. */
    gp = PMContext_get_grouplist(pm_ctx);
    while (gp) {
        if (gp->cpmv.type == PMV_seg) {
            /* For each segment vertex group, create clusters of equivalent
             * segments. */
            SegClusterCtx *cluster_ctx = SegClusterCtx_create(gp, PARAM_K, 0);
            if (!cluster_ctx) {
                printf("Seg Cluster: cannot create cluster.\n");
                return -1;
            }
            /* Since all the segments in a cluster are forming an equivalence
             * class, only one representative is required to be preserved. This
             * step removes all the duplicates and updates the segment vertices
             * in the PPM tree accordingly. */
            SegClusterCtx_remdupl(cluster_ctx);
            SegClusterCtx_destroy(cluster_ctx);
        }
        gp = (PMVG*)((Elem*)gp)->next_p;
    }
```

### Graph compression and compressed model serialization

These both steps are handled into the same routine. The graph compression only links the segment vertex groups together to form a new graph. The references to the task segments are kept in an array, stored in a DFS-pre-order manner (see thesis).    
All the objects that need to be serialized inherit the `Elem` abstract class, so they are easy to access trough their context. Part of the serialization is to convert object references to indices, which is also possible trough their context. The structure of the serialized file is found in `abstract_utils.c` and in the class-specific serialization routines.

```c
    FILE *mod_outf_comp = fopen(model_out_fname_comp, "w");
    if (!mod_outf_comp) {
        printf("Error opening compressed destination file %s.\n",
                model_out_fname_comp);
        return -1;
    }

    /* Export the binary uncompressed model.*/
    int fsize_comp =
            au_export_model(dict_ctx, (TaskSegCtx*)tsb_ctx, pm_ctx, mod_outf_comp);
    if (fsize_comp < 0) {
        printf("Error exporting uncompressed model.\n");
        return -1;
    }

    fclose(mod_outf_comp);
```

We can now perform the clean-up routines.
```c
    Object_deinit((Object*)tsb_ctx);
    _obj_free(tsb_ctx);
    Object_deinit((Object*)dict_ctx);
    _obj_free(dict_ctx);
    PMContext_destroy(pm_ctx);
```

## Implementing additional tree-mining methods
The tree mining methods are implemented in `graph_miner.c`. Additional
tree mining implementation can use the tools available in `pm.h` and documented in `pm.c`.