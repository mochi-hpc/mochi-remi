#include <stdio.h>
#include <margo.h>
#include <remi/remi-server.h>

void my_fileset_printer(const char* filename, void* uarg) {
    (void)uarg;
    fprintf(stdout,"   - %s\n", filename);
}

void my_metadata_printer(const char* key, const char* value, void* uarg) {
    (void)uarg;
    fprintf(stdout,"   - %s\t==>\t%s\n", key, value);
}

int my_pre_migration_callback(remi_fileset_t fileset, void* uargs) {
    fprintf(stdout, "Migration starting.\n");
    return 0;
}

int my_post_migration_callback(remi_fileset_t fileset, void* uargs) {
    fprintf(stdout, "Migration terminated.\n");
    fprintf(stdout, "The following files were transfered:\n");
    remi_fileset_foreach_file(fileset, my_fileset_printer, NULL);
    fprintf(stdout, "Associated metadata:\n");
    remi_fileset_foreach_metadata(fileset, my_metadata_printer, NULL);
    return 0;
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <listen-address>\n", argv[0]);
        return -1;
    }

    hg_return_t hret          = HG_SUCCESS;
    int ret                   = 0;
    char* listen_addr_str     = argv[1];
    uint16_t provider_id      = 1;
    margo_instance_id mid     = MARGO_INSTANCE_NULL;
    remi_provider_t remi_prov = REMI_PROVIDER_NULL;
    hg_addr_t my_addr         = HG_ADDR_NULL;

    // initialize margo
    mid = margo_init(listen_addr_str, MARGO_SERVER_MODE, 0, -1);
    if(mid == MARGO_INSTANCE_NULL) {
        return -1;
    }
    margo_enable_remote_shutdown(mid);

    // get the address we are listening on
    char my_addr_str[256] = {0};
    size_t my_addr_size = 256;
    hret = margo_addr_self(mid, &my_addr);
    if(hret != HG_SUCCESS) {
        fprintf(stderr, "ERROR: margo_addr_self() returned %d\n", hret);
        ret = -1;
        goto error;
    }
    hret = margo_addr_to_string(mid, my_addr_str, &my_addr_size, my_addr);
    if(hret != HG_SUCCESS) {
        fprintf(stderr, "ERROR: margo_addr_to_string() returned %d\n", hret);
        ret = -1;
        margo_addr_free(mid, my_addr);
        goto error;
    }
    margo_addr_free(mid, my_addr);
    fprintf(stdout,"Server running at address %s\n", my_addr_str);

    // create the REMI provider
    ret = remi_provider_register(mid, 1, REMI_ABT_POOL_DEFAULT, &remi_prov);
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_provider_register() returned %d\n", ret);
        ret = -1;
        goto error;
    }

    // create migration class
    ret = remi_provider_register_migration_class(remi_prov, 
            "my_migration_class", 
            my_pre_migration_callback,
            my_post_migration_callback, NULL, NULL);
    if(ret != REMI_SUCCESS) {
        fprintf(stderr, "ERROR: remi_provider_register_migration_class() returned %d\n", ret);
        ret = -1;
        goto error;
    }

    // make margo wait for finalize
    margo_wait_for_finalize(mid);

finish:
    return ret;
error:
    margo_finalize(mid);
    goto finish;
}
