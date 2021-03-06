/*
 * qemu_conf.h: QEMU configuration management
 *
 * Copyright (C) 2006-2007, 2009-2013 Red Hat, Inc.
 * Copyright (C) 2006 Daniel P. Berrange
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */

#ifndef __QEMUD_CONF_H
# define __QEMUD_CONF_H

# include <config.h>

# include "virebtables.h"
# include "internal.h"
# include "capabilities.h"
# include "network_conf.h"
# include "domain_conf.h"
# include "domain_event.h"
# include "virthread.h"
# include "security/security_manager.h"
# include "vircgroup.h"
# include "virpci.h"
# include "virusb.h"
# include "cpu_conf.h"
# include "driver.h"
# include "virportallocator.h"
# include "vircommand.h"
# include "virthreadpool.h"
# include "locking/lock_manager.h"
# include "qemu_capabilities.h"

# define QEMUD_CPUMASK_LEN CPU_SETSIZE

typedef struct _virQEMUCloseCallbacks virQEMUCloseCallbacks;
typedef virQEMUCloseCallbacks *virQEMUCloseCallbacksPtr;

typedef struct _virQEMUDriver virQEMUDriver;
typedef virQEMUDriver *virQEMUDriverPtr;

typedef struct _virQEMUDriverConfig virQEMUDriverConfig;
typedef virQEMUDriverConfig *virQEMUDriverConfigPtr;

/* Main driver config. The data in these object
 * instances is immutable, so can be accessed
 * without locking. Threads must, however, hold
 * a valid reference on the object to prevent it
 * being released while they use it.
 *
 * eg
 *  qemuDriverLock(driver);
 *  virQEMUDriverConfigPtr cfg = virObjectRef(driver->config);
 *  qemuDriverUnlock(driver);
 *
 *  ...do stuff with 'cfg'..
 *
 *  virObjectUnref(cfg);
 */
struct _virQEMUDriverConfig {
    virObject parent;

    bool privileged;
    const char *uri;

    uid_t user;
    gid_t group;
    int dynamicOwnership;

    int cgroupControllers;
    char **cgroupDeviceACL;

    /* These five directories are ones libvirtd uses (so must be root:root
     * to avoid security risk from QEMU processes */
    char *configBaseDir;
    char *configDir;
    char *autostartDir;
    char *logDir;
    char *stateDir;
    /* These two directories are ones QEMU processes use (so must match
     * the QEMU user/group */
    char *libDir;
    char *cacheDir;
    char *saveDir;
    char *snapshotDir;

    bool vncAutoUnixSocket;
    bool vncTLS;
    bool vncTLSx509verify;
    bool vncSASL;
    char *vncTLSx509certdir;
    char *vncListen;
    char *vncPassword;
    char *vncSASLdir;

    bool spiceTLS;
    char *spiceTLSx509certdir;
    char *spiceListen;
    char *spicePassword;

    int remotePortMin;
    int remotePortMax;

    char *hugetlbfsMount;
    char *hugepagePath;

    bool macFilter;

    bool relaxedACS;
    bool vncAllowHostAudio;
    bool clearEmulatorCapabilities;
    bool allowDiskFormatProbing;
    bool setProcessName;

    int maxProcesses;
    int maxFiles;

    int maxQueuedJobs;

    char **securityDriverNames;
    bool securityDefaultConfined;
    bool securityRequireConfined;

    char *saveImageFormat;
    char *dumpImageFormat;

    char *autoDumpPath;
    bool autoDumpBypassCache;
    bool autoStartBypassCache;

    char *lockManagerName;

    int keepAliveInterval;
    unsigned int keepAliveCount;

    int seccompSandbox;
};

/* Main driver state */
struct _virQEMUDriver {
    virMutex lock;

    /* Require lock to get reference on 'config',
     * then lockless thereafter */
    virQEMUDriverConfigPtr config;

    /* Immutable pointer, self-locking APIs */
    virThreadPoolPtr workerPool;

    /* Atomic increment only */
    int nextvmid;

    /* Immutable pointer. Immutable object */
    virCgroupPtr cgroup;

    /* Atomic inc/dec only */
    unsigned int nactive;

    /* Immutable pointers. Caller must provide locking */
    virStateInhibitCallback inhibitCallback;
    void *inhibitOpaque;

    /* Immutable pointer, self-locking APIs */
    virDomainObjListPtr domains;

    /* Immutable pointer */
    char *qemuImgBinary;

    /* Immutable pointer, lockless APIs. Pointless abstraction */
    ebtablesContext *ebtables;

    /* Require lock to get a reference on the object,
     * lockless access thereafter
     */
    virCapsPtr caps;

    /* Immutable pointer, self-locking APIs */
    virQEMUCapsCachePtr qemuCapsCache;

    /* Immutable pointer, self-locking APIs */
    virDomainEventStatePtr domainEventState;

    /* Immutable pointer. self-locking APIs */
    virSecurityManagerPtr securityManager;

    /* Immutable pointers. Requires locks to be held before
     * calling APIs. activePciHostdevs must be locked before
     * inactivePciHostdevs */
    virPCIDeviceListPtr activePciHostdevs;
    virPCIDeviceListPtr inactivePciHostdevs;
    virUSBDeviceListPtr activeUsbHostdevs;

    /* Immutable pointer. Unsafe APIs. XXX */
    virHashTablePtr sharedDisks;

    /* Immutable pointer, self-locking APIs */
    virPortAllocatorPtr remotePorts;

    /* Immutable pointer, lockless APIs*/
    virSysinfoDefPtr hostsysinfo;

    /* Immutable pointer. lockless access */
    virLockManagerPluginPtr lockManager;

    /* Immutable pointer, self-clocking APIs */
    virQEMUCloseCallbacksPtr closeCallbacks;
};

typedef struct _qemuDomainCmdlineDef qemuDomainCmdlineDef;
typedef qemuDomainCmdlineDef *qemuDomainCmdlineDefPtr;
struct _qemuDomainCmdlineDef {
    unsigned int num_args;
    char **args;

    unsigned int num_env;
    char **env_name;
    char **env_value;
};

/* Port numbers used for KVM migration. */
# define QEMUD_MIGRATION_FIRST_PORT 49152
# define QEMUD_MIGRATION_NUM_PORTS 64


virQEMUDriverConfigPtr virQEMUDriverConfigNew(bool privileged);

int virQEMUDriverConfigLoadFile(virQEMUDriverConfigPtr cfg,
                                const char *filename);

virQEMUDriverConfigPtr virQEMUDriverGetConfig(virQEMUDriverPtr driver);

virCapsPtr virQEMUDriverCreateCapabilities(virQEMUDriverPtr driver);
virCapsPtr virQEMUDriverGetCapabilities(virQEMUDriverPtr driver,
                                        bool refresh);

struct qemuDomainDiskInfo {
    bool removable;
    bool locked;
    bool tray_open;
    int io_status;
};

/*
 * To avoid a certain deadlock this callback must never call any
 * virQEMUCloseCallbacks* API.
 */
typedef virDomainObjPtr (*virQEMUCloseCallback)(virQEMUDriverPtr driver,
                                                virDomainObjPtr vm,
                                                virConnectPtr conn);
virQEMUCloseCallbacksPtr virQEMUCloseCallbacksNew(void);
int virQEMUCloseCallbacksSet(virQEMUCloseCallbacksPtr closeCallbacks,
                             virDomainObjPtr vm,
                             virConnectPtr conn,
                             virQEMUCloseCallback cb);
int virQEMUCloseCallbacksUnset(virQEMUCloseCallbacksPtr closeCallbacks,
                               virDomainObjPtr vm,
                               virQEMUCloseCallback cb);
virQEMUCloseCallback
virQEMUCloseCallbacksGet(virQEMUCloseCallbacksPtr closeCallbacks,
                         virDomainObjPtr vm,
                         virConnectPtr conn);
void virQEMUCloseCallbacksRun(virQEMUCloseCallbacksPtr closeCallbacks,
                              virConnectPtr conn,
                              virQEMUDriverPtr driver);

typedef struct _qemuSharedDiskEntry qemuSharedDiskEntry;
typedef qemuSharedDiskEntry *qemuSharedDiskEntryPtr;

bool qemuSharedDiskEntryDomainExists(qemuSharedDiskEntryPtr entry,
                                     const char *name,
                                     int *index)
    ATTRIBUTE_NONNULL(1) ATTRIBUTE_NONNULL(2);

int qemuAddSharedDisk(virQEMUDriverPtr driver,
                      virDomainDiskDefPtr disk,
                      const char *name)
    ATTRIBUTE_NONNULL(1) ATTRIBUTE_NONNULL(2) ATTRIBUTE_NONNULL(3);

int qemuRemoveSharedDisk(virQEMUDriverPtr driver,
                         virDomainDiskDefPtr disk,
                         const char *name)
    ATTRIBUTE_NONNULL(1) ATTRIBUTE_NONNULL(2) ATTRIBUTE_NONNULL(3);

char * qemuGetSharedDiskKey(const char *disk_path)
    ATTRIBUTE_NONNULL(1);

void qemuSharedDiskEntryFree(void *payload, const void *name)
    ATTRIBUTE_NONNULL(1);

int qemuDriverAllocateID(virQEMUDriverPtr driver);

#endif /* __QEMUD_CONF_H */
