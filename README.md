REsource Migration Interface
============================

REMI is a Mochi microservice designed to handle the migration of sets of files
from a node to another. It uses RDMA and memory mapping to efficiently transfer
potentially large groups of files at once.

### Installing

Just like all Mochi services, REMI can be installed using Spack. Once you have
clone the [sds-repo](https://xgitlab.cels.anl.gov/sds/sds-repo) package repository
and added it to your spack installation, you can install REMI using the following
command:

```
spack install mochi-remi
```

REMI depends on [Thallium](https://xgitlab.cels.anl.gov/sds/thallium/), which
Spack will install (if needed) along with Thallium's own dependencies. It also
depends on Bedrock, unless the `bedrock` variant is disable when installing
with Spack (i.e. passing `~bedrock` to the above command).

### Overview

REMI works with _filesets_. A fileset consists of a root directory and
a set of file paths relative to this root directory. A fileset is also characterized
by the name of its _migration class_.

REMI clients create filesets to group files corresponding to a particular resource
(e.g. a database's files). They can then request the migration of fileset to
a target provider.

Uppon receiving a request for migration, a provider will recreate the tree of
directories required to receive the files of the fileset, create the files,
mmap them into memory, and issue an RDMA pull operation from the client's files
(themselves mmap-ed into the client's memory).

Following successful migration, the provider will call a user-supplied callback
corresponding to the particular fileset's migration class.

For an example of code, please see the [examples](https://xgitlab.cels.anl.gov/sds/remi/tree/master/examples)
folder in the source tree.
