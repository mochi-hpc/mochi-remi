#include <margo.h>
#include <remi/remi-client.h>

int main(int argc, char** argv)
{
    if(argc < 5) {
        fprintf(stderr, "Usage: %s <server-address> <local-root> <dest-root> file1 [ file2 ...\n", argv[0]);
        return -1;
    }

    int ret                        = 0;
    hg_return_t hret               = HG_SUCCESS;
    char** filenames               = argv+4;
    unsigned num_files             = argc-4;
    char* server_addr_str          = argv[1];
    char* local_root               = argv[2];
    char* remote_root              = argv[3];
    margo_instance_id mid          = MARGO_INSTANCE_NULL;
    remi_client_t remi_clt         = REMI_CLIENT_NULL;
    remi_provider_handle_t remi_ph = REMI_PROVIDER_HANDLE_NULL;
    hg_addr_t svr_addr             = HG_ADDR_NULL;
    remi_fileset_t fileset         = REMI_FILESET_NULL;

    // initialize margo
    unsigned i;
    char cli_addr_prefix[64] = {0};
    for(i=0; (i<63 && server_addr_str[i] != '\0' && server_addr_str[i] != ':'); i++)
        cli_addr_prefix[i] = server_addr_str[i];

    mid = margo_init(cli_addr_prefix, MARGO_CLIENT_MODE, 0, 0);
    if(mid == MARGO_INSTANCE_NULL) {
        fprintf(stderr, "ERROR: margo_initialize()\n");
        ret = -1;
        goto error;
    }

    // initialize REMI client
    ret = remi_client_init(mid, &remi_clt);
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_client_init() returned %d\n", ret);
        ret = -1;
        goto error;
    }

    // lookup server address
    hret = margo_addr_lookup(mid, server_addr_str, &svr_addr);
    if(hret != HG_SUCCESS) {
        fprintf(stderr, "ERROR: margo_addr_lookup() returned %d\n", hret);
        ret = -1;
        goto error;
    }

    // create REMI provider handle
    ret = remi_provider_handle_create(remi_clt, svr_addr, 1, &remi_ph);
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_provider_handle_create() returned %d\n", ret);
        ret = -1;
        goto error;
    }

    // create a fileset
    ret = remi_fileset_create("my_migration_class", local_root, &fileset);
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_fileset_create() returned %d\n", ret);
        ret = -1;
        goto error;
    }

    // fill the fileset with file paths
    for(i=0; i < num_files; i++) {
        ret = remi_fileset_register_file(fileset, filenames[i]);
        if(ret != REMI_SUCCESS) {
            fprintf(stderr, "ERROR: remi_fileset_register_file() returned %d\n", ret);
            ret = -1;
            goto error;
        }
    }

    // fill the fileset with some metadata
    ret = remi_fileset_register_metadata(fileset, "ABC", "DEF");
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_fileset_register_metadata() returned %d\n", ret);
        ret = -1;
        goto error;
    }
    remi_fileset_register_metadata(fileset, "AERED", "qerqwer");

    // migrate the fileset
    int status = 0;
    ret =  remi_fileset_migrate(remi_ph, fileset, remote_root, REMI_KEEP_SOURCE, &status);
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_fileset_migrate() returned %d\n", ret);
        if(ret == REMI_ERR_USER) {
            fprintf(stderr, "-----  user error: %d\n", status);
        }
        ret = -1;
        goto error;
    }

    // shut down the server
    remi_shutdown_service(remi_clt, svr_addr);

finish:
    // cleanup
    remi_fileset_free(fileset);
    remi_provider_handle_release(remi_ph);
    margo_addr_free(mid, svr_addr);
    remi_client_finalize(remi_clt);
    margo_finalize(mid);
    return ret;
error:
    goto finish;
}
