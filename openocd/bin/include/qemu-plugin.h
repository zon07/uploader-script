/*
 * Copyright (C) 2017, Emilio G. Cota <cota@braap.org>
 * Copyright (C) 2019, Linaro
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_QEMU_PLUGIN_H
#define QEMU_QEMU_PLUGIN_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * For best performance, build the plugin with -fvisibility=hidden so that
 * QEMU_PLUGIN_LOCAL is implicit. Then, just mark qemu_plugin_install with
 * QEMU_PLUGIN_EXPORT. For more info, see
 *   https://gcc.gnu.org/wiki/Visibility
 */
#if defined _WIN32 || defined __CYGWIN__
  #ifdef CONFIG_PLUGIN
    #define QEMU_PLUGIN_EXPORT __declspec(dllimport)
    #define QEMU_PLUGIN_API __declspec(dllexport)
  #else
    #define QEMU_PLUGIN_EXPORT __declspec(dllexport)
    #define QEMU_PLUGIN_API __declspec(dllimport)
  #endif
  #define QEMU_PLUGIN_LOCAL
#else
  #define QEMU_PLUGIN_EXPORT __attribute__((visibility("default")))
  #define QEMU_PLUGIN_LOCAL  __attribute__((visibility("hidden")))
  #define QEMU_PLUGIN_API
#endif

/**
 * typedef qemu_plugin_id_t - Unique plugin ID
 */
typedef uint64_t qemu_plugin_id_t;

/*
 * Versioning plugins:
 *
 * The plugin API will pass a minimum and current API version that
 * QEMU currently supports. The minimum API will be incremented if an
 * API needs to be deprecated.
 *
 * The plugins export the API they were built against by exposing the
 * symbol qemu_plugin_version which can be checked.
 */

extern QEMU_PLUGIN_EXPORT int qemu_plugin_version;

#define QEMU_PLUGIN_VERSION 7

/**
 * struct qemu_info_t - system information for plugins
 *
 * This structure provides for some limited information about the
 * system to allow the plugin to make decisions on how to proceed. For
 * example it might only be suitable for running on some guest
 * architectures or when under full system emulation.
 */
typedef struct qemu_info_t {
    /** @target_name: string describing architecture */
    const char *target_name;
    /** @version: minimum and current plugin API level */
    struct {
        int min;
        int cur;
    } version;
    /** @system_emulation: is this a full system emulation? */
    bool system_emulation;
    union {
        /** @system: information relevant to system emulation */
        struct {
            /** @system.smp_vcpus: initial number of vCPUs */
            int smp_vcpus;
            /** @system.max_vcpus: maximum possible number of vCPUs */
            int max_vcpus;
        } system;
    };

    /** @mttcg_enabled: is TCG running in multi-threaded mode? */
    bool mttcg_enabled;
} qemu_info_t;

/**
 * qemu_plugin_install() - Install a plugin
 * @id: this plugin's opaque ID
 * @info: a block describing some details about the guest
 * @argc: number of arguments
 * @argv: array of arguments (@argc elements)
 *
 * All plugins must export this symbol which is called when the plugin
 * is first loaded. Calling qemu_plugin_uninstall() from this function
 * is a bug.
 *
 * Note: @info is only live during the call. Copy any information we
 * want to keep. @argv remains valid throughout the lifetime of the
 * loaded plugin.
 *
 * Return: 0 on successful loading, !0 for an error.
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv);

/**
 * typedef qemu_plugin_simple_cb_t - simple callback
 * @id: the unique qemu_plugin_id_t
 *
 * This callback passes no information aside from the unique @id.
 */
typedef void (*qemu_plugin_simple_cb_t)(qemu_plugin_id_t id);

/**
 * typedef qemu_plugin_udata_cb_t - callback with user data
 * @id: the unique qemu_plugin_id_t
 * @userdata: a pointer to some user data supplied when the callback
 * was registered.
 */
typedef void (*qemu_plugin_udata_cb_t)(qemu_plugin_id_t id, void *userdata);

/**
 * typedef qemu_plugin_vcpu_simple_cb_t - vcpu callback
 * @id: the unique qemu_plugin_id_t
 * @vcpu_index: the current vcpu context
 */
typedef void (*qemu_plugin_vcpu_simple_cb_t)(qemu_plugin_id_t id,
                                             unsigned int vcpu_index);

/**
 * typedef qemu_plugin_vcpu_udata_cb_t - vcpu callback
 * @vcpu_index: the current vcpu context
 * @userdata: a pointer to some user data supplied when the callback
 * was registered.
 */
typedef void (*qemu_plugin_vcpu_udata_cb_t)(unsigned int vcpu_index,
                                            void *userdata);

/**
 * qemu_plugin_uninstall() - Uninstall a plugin
 * @id: this plugin's opaque ID
 * @cb: callback to be called once the plugin has been removed
 *
 * Do NOT assume that the plugin has been uninstalled once this function
 * returns. Plugins are uninstalled asynchronously, and therefore the given
 * plugin receives callbacks until @cb is called.
 *
 * Note: Calling this function from qemu_plugin_install() is a bug.
 */
QEMU_PLUGIN_API
void qemu_plugin_uninstall(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);

/**
 * qemu_plugin_reset() - Reset a plugin
 * @id: this plugin's opaque ID
 * @cb: callback to be called once the plugin has been reset
 *
 * Unregisters all callbacks for the plugin given by @id.
 *
 * Do NOT assume that the plugin has been reset once this function returns.
 * Plugins are reset asynchronously, and therefore the given plugin receives
 * callbacks until @cb is called.
 */
QEMU_PLUGIN_API
void qemu_plugin_reset(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);

/**
 * qemu_plugin_register_vcpu_init_cb() - register a vCPU initialization callback
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called every time a vCPU is initialized.
 *
 * See also: qemu_plugin_register_vcpu_exit_cb()
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_init_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);

/**
 * qemu_plugin_register_vcpu_exit_cb() - register a vCPU exit callback
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called every time a vCPU exits.
 *
 * See also: qemu_plugin_register_vcpu_init_cb()
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_exit_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);

/**
 * qemu_plugin_register_vcpu_idle_cb() - register a vCPU idle callback
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called every time a vCPU idles.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_idle_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);

/**
 * qemu_plugin_register_vcpu_resume_cb() - register a vCPU resume callback
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called every time a vCPU resumes execution.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_resume_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_simple_cb_t cb);

/**
 * qemu_plugin_register_vcpu_interrupt_cb() - register a vCPU interrupt callback
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called every time an interrupt is triggered on given vCPU.
 */
void qemu_plugin_register_vcpu_interrupt_cb(qemu_plugin_id_t id,
                                            qemu_plugin_vcpu_simple_cb_t cb);

/** struct qemu_plugin_tb - Opaque handle for a translation block */
struct qemu_plugin_tb;
/** struct qemu_plugin_insn - Opaque handle for a translated instruction */
struct qemu_plugin_insn;

/**
 * enum qemu_plugin_cb_flags - type of callback
 *
 * @QEMU_PLUGIN_CB_NO_REGS: callback does not access the CPU's regs
 * @QEMU_PLUGIN_CB_R_REGS: callback reads the CPU's regs
 * @QEMU_PLUGIN_CB_RW_REGS: callback reads and writes the CPU's regs
 *
 * Note: currently unused, plugins cannot read or change system
 * register state.
 */
enum qemu_plugin_cb_flags {
    QEMU_PLUGIN_CB_NO_REGS,
    QEMU_PLUGIN_CB_R_REGS,
    QEMU_PLUGIN_CB_RW_REGS,
};

/**
 * enum qemu_plugin_tb_flags - type of translation block
 *
 * @QEMU_PLUGIN_TB_MEM_ONLY:
 *  TB is special block to perform memory I/O operation only.
 *  Block- and instruction- level callbacks have no effect.
 * @QEMU_PLUGIN_TB_MEM_OPS:
 *  TB has at least one instruction that access memory.
 *  Memory callbacks are applicable to this TB.
 */
enum qemu_plugin_tb_flags {
    QEMU_PLUGIN_TB_MEM_ONLY = 0x01,
    QEMU_PLUGIN_TB_MEM_OPS = 0x02
};

enum qemu_plugin_mem_rw {
    QEMU_PLUGIN_MEM_R = 1,
    QEMU_PLUGIN_MEM_W,
    QEMU_PLUGIN_MEM_RW,
};

enum qemu_plugin_disas_syntax {
    QEMU_PLUGIN_DISAS_SYNTAX_DEFAULT,
    QEMU_PLUGIN_DISAS_SYNTAX_ATT,
    QEMU_PLUGIN_DISAS_SYNTAX_INTEL,
    QEMU_PLUGIN_DISAS_SYNTAX_MASM,
};

/**
 * typedef qemu_plugin_vcpu_tb_trans_cb_t - translation callback
 * @id: unique plugin id
 * @tb: opaque handle used for querying and instrumenting a block.
 */
typedef void (*qemu_plugin_vcpu_tb_trans_cb_t)(qemu_plugin_id_t id,
                                               struct qemu_plugin_tb *tb);

/**
 * typedef qemu_plugin_tb_invalidate_cb_t - tb invalidation callback
 * @hash: block hash
 * @userdata: a pointer to some user data supplied when the callback
 * was registered.
 */
typedef void (*qemu_plugin_tb_invalidate_cb_t)(uint32_t hash,
                                               void *userdata);

/**
 * qemu_plugin_register_vcpu_tb_trans_cb() - register a translate cb
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called every time a translation occurs. The @cb
 * function is passed an opaque qemu_plugin_type which it can query
 * for additional information including the list of translated
 * instructions. At this point the plugin can register further
 * callbacks to be triggered when the block or individual instruction
 * executes.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_trans_cb(qemu_plugin_id_t id,
                                           qemu_plugin_vcpu_tb_trans_cb_t cb);

/**
 * qemu_plugin_register_vcpu_tb_exec_cb() - register execution callback
 * @tb: the opaque qemu_plugin_tb handle for the translation
 * @cb: callback function
 * @flags: does the plugin read or write the CPU's registers?
 * @userdata: any plugin data to pass to the @cb?
 *
 * The @cb function is called every time a translated unit executes.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_exec_cb(struct qemu_plugin_tb *tb,
                                          qemu_plugin_vcpu_udata_cb_t cb,
                                          enum qemu_plugin_cb_flags flags,
                                          void *userdata);

/**
 * qemu_plugin_register_tb_invalidate_cb() - register invalidation callback
 * @tb: the opaque qemu_plugin_tb handle for the translation
 * @cb: callback function
 * @userdata: any plugin data to pass to the @cb?
 *
 * The @cb function is called every time a tb invalidation occurs. For example,
 * invalidation happens if current TB is rebuilt (modified) or cache buffer is full.
 * This means that references to this TB are invalid
 */
void qemu_plugin_register_tb_invalidate_cb(struct qemu_plugin_tb *tb,
    qemu_plugin_tb_invalidate_cb_t cb, void *userdata);

/**
 * qemu_plugin_tb_hash() - query helper for hash of TB
 * @tb: opaque handle to TB passed to callback
 *
 * Returns: block hash
 */
uint32_t qemu_plugin_tb_hash(struct qemu_plugin_tb *tb);

/**
 * enum qemu_plugin_op - describes an inline op
 *
 * @QEMU_PLUGIN_INLINE_ADD_U64: add an immediate value uint64_t
 * @QEMU_PLUGIN_INLINE_ADD_U64_ATOMIC: add an immediate value atomic uint64_t
 *
 * Note: currently only a single inline op is supported.
 */

enum qemu_plugin_op {
    QEMU_PLUGIN_INLINE_ADD_U64,
    QEMU_PLUGIN_INLINE_ADD_U64_ATOMIC,
};

/**
 * qemu_plugin_register_vcpu_tb_exec_inline() - execution inline op
 * @tb: the opaque qemu_plugin_tb handle for the translation
 * @op: the type of qemu_plugin_op (e.g. ADD_U64)
 * @ptr: the target memory location for the op
 * @imm: the op data (e.g. 1)
 *
 * Insert an inline op to every time a translated unit executes.
 * Useful if you just want to increment a single counter somewhere in
 * memory.
 *
 * Note: ops are not atomic so in multi-threaded/multi-smp situations
 * you will get inexact results.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_exec_inline(struct qemu_plugin_tb *tb,
                                              enum qemu_plugin_op op,
                                              void *ptr, uint64_t imm);

#define QEMU_PLUGIN_CB_AFTER_INSN_EXEC_ENABLED

/**
 * qemu_plugin_register_vcpu_insn_exec_cb() - register insn execution cb
 * @insn: the opaque qemu_plugin_insn handle for an instruction
 * @cb: callback function
 * @flags: does the plugin read or write the CPU's registers?
 * @userdata: any plugin data to pass to the @cb?
 *
 * The @cb function is called every time an instruction is executed
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_insn_exec_cb(struct qemu_plugin_insn *insn,
                                            qemu_plugin_vcpu_udata_cb_t cb,
                                            enum qemu_plugin_cb_flags flags,
                                            void *userdata);

/**
 * qemu_plugin_register_vcpu_after_insn_exec_cb() - register cb
 * after insn execution
 * @insn: the opaque qemu_plugin_insn handle for an instruction
 * @cb: callback function
 * @flags: does the plugin read or write the CPU's registers?
 * @userdata: any plugin data to pass to the @cb?
 *
 * The @cb function is called every time after a non-control-flow
 * instruction is executed
 */
void qemu_plugin_register_vcpu_after_insn_exec_cb(
    struct qemu_plugin_insn *insn, qemu_plugin_vcpu_udata_cb_t cb,
    enum qemu_plugin_cb_flags flags, void *userdata);

/**
 * qemu_plugin_register_vcpu_insn_exec_inline() - insn execution inline op
 * @insn: the opaque qemu_plugin_insn handle for an instruction
 * @op: the type of qemu_plugin_op (e.g. ADD_U64)
 * @ptr: the target memory location for the op
 * @imm: the op data (e.g. 1)
 *
 * Insert an inline op to every time an instruction executes. Useful
 * if you just want to increment a single counter somewhere in memory.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_insn_exec_inline(struct qemu_plugin_insn *insn,
                                                enum qemu_plugin_op op,
                                                void *ptr, uint64_t imm);

/**
 * qemu_plugin_register_vcpu_after_insn_exec_inline() - after insn execution
 * inline op
 * @insn: the opaque qemu_plugin_insn handle for an instruction
 * @op: the type of qemu_plugin_op (e.g. ADD_U64)
 * @ptr: the target memory location for the op
 * @imm: the op data (e.g. 1)
 *
 * Insert an inline op to every time after a non-control-flow
 * instruction executes.
 * Useful if you just want to increment a single counter somewhere in memory.
 */
void qemu_plugin_register_vcpu_after_insn_exec_inline(
    struct qemu_plugin_insn *insn, enum qemu_plugin_op op,
    void *ptr, uint64_t imm);

/**
 * qemu_plugin_tb_n_insns() - query helper for number of insns in TB
 * @tb: opaque handle to TB passed to callback
 *
 * Returns: number of instructions in this block
 */
QEMU_PLUGIN_API
size_t qemu_plugin_tb_n_insns(const struct qemu_plugin_tb *tb);

/**
 * qemu_plugin_tb_vaddr() - query helper for vaddr of TB start
 * @tb: opaque handle to TB passed to callback
 *
 * Returns: virtual address of block start
 */
QEMU_PLUGIN_API
uint64_t qemu_plugin_tb_vaddr(const struct qemu_plugin_tb *tb);

/**
 * qemu_plugin_tb_get_flags() - returns combination of TB flags
 * @tb: opaque handle to TB passed to callback
 *
 * Returned set of flags can be used to check if TB has a non-typical
 * behaviour. For example: whether or not instruction execution
 * callbacks are applicable for the block.
 *
 * Returns: 0 or combination of qemu_plugin_tb_flags
 */
int qemu_plugin_tb_get_flags(const struct qemu_plugin_tb *tb);

/**
 * qemu_plugin_tb_get_insn() - retrieve handle for instruction
 * @tb: opaque handle to TB passed to callback
 * @idx: instruction number, 0 indexed
 *
 * The returned handle can be used in follow up helper queries as well
 * as when instrumenting an instruction. It is only valid for the
 * lifetime of the callback.
 *
 * Returns: opaque handle to instruction
 */
QEMU_PLUGIN_API
struct qemu_plugin_insn *
qemu_plugin_tb_get_insn(const struct qemu_plugin_tb *tb, size_t idx);

/**
 * qemu_plugin_insn_data() - return ptr to instruction data
 * @insn: opaque instruction handle from qemu_plugin_tb_get_insn()
 *
 * Note: data is only valid for duration of callback. See
 * qemu_plugin_insn_size() to calculate size of stream.
 *
 * Returns: pointer to a stream of bytes containing the value of this
 * instructions opcode.
 */
QEMU_PLUGIN_API
const void *qemu_plugin_insn_data(const struct qemu_plugin_insn *insn);

/**
 * qemu_plugin_insn_size() - return size of instruction
 * @insn: opaque instruction handle from qemu_plugin_tb_get_insn()
 *
 * Returns: size of instruction in bytes
 */
QEMU_PLUGIN_API
size_t qemu_plugin_insn_size(const struct qemu_plugin_insn *insn);

/**
 * qemu_plugin_insn_vaddr() - return vaddr of instruction
 * @insn: opaque instruction handle from qemu_plugin_tb_get_insn()
 *
 * Returns: virtual address of instruction
 */
QEMU_PLUGIN_API
uint64_t qemu_plugin_insn_vaddr(const struct qemu_plugin_insn *insn);

/**
 * qemu_plugin_insn_haddr() - return hardware addr of instruction
 * @insn: opaque instruction handle from qemu_plugin_tb_get_insn()
 *
 * Returns: hardware (physical) target address of instruction
 */
QEMU_PLUGIN_API
void *qemu_plugin_insn_haddr(const struct qemu_plugin_insn *insn);

#define QEMU_PLUGIN_REGISTERS_ENABLED

/**
 * qemu_plugin_find_reg() - find register by name
 * @name: register name
 * @regnum: a pointer to store register number
 *
 * Returns: true if found, false otherwise
 */
bool qemu_plugin_find_reg(const char *name, int *regnum);

/**
 * qemu_plugin_get_available_reg_names() - write the names of all
 * available registers for the current CPU to destination buffer
 * @buf: destination buffer to write data, can be NULL
 * @buf_size: destination buffer size, can be 0
 *
 * The register names in the buffer are separated by commas
 *
 * Returns: the size of the data in bytes written to the buffer.
 * Or if @buf is NULL, the size of the buffer needed to write data
 */
size_t qemu_plugin_get_available_reg_names(char *buf, size_t buf_size);

/**
 * qemu_plugin_read_reg() - return register data
 * @regnum: register number
 * @size: a pointer to store allocated memory size
 *
 * Returns: allocated memory containing register data,
 * memory must be freed manually using g_free()
 */
const void *qemu_plugin_read_reg(int regnum, size_t *size);

/** struct qemu_plugin_reg_ctx - Opaque handle for register context */
struct qemu_plugin_reg_ctx;

/**
 * qemu_plugin_n_regs() - query helper for number of registers in context
 * @ctx: register context
 *
 * Returns: number of registers in context
 */
size_t qemu_plugin_n_regs(const struct qemu_plugin_reg_ctx *ctx);

/**
 * qemu_plugin_reg_create_context() - create a context for working
 * with registers
 * @names: array with register names
 * @len: array length
 *
 * Limitation: cannot be called from init_cpu callback
 *
 * Returns: opaque handle to register context
 */
struct qemu_plugin_reg_ctx *
qemu_plugin_reg_create_context(const char *const *names,
                               size_t len);

/**
 * qemu_plugin_reg_free_context() - free the register context
 * @ctx: register context
 */
void qemu_plugin_reg_free_context(struct qemu_plugin_reg_ctx *ctx);

/**
 * qemu_plugin_reg_name() - query helper for register name in context
 * @ctx: register context
 * @idx: register index
 *
 * Returns: returns register name
 */
const char *qemu_plugin_reg_name(const struct qemu_plugin_reg_ctx *ctx,
                                 size_t idx);

/**
 * qemu_plugin_reg_ptr() - query helper for a pointer to register data
 * in context
 * @ctx: register context
 * @idx: register index
 *
 * Returns: pointer to register data
 */
const void *qemu_plugin_reg_ptr(const struct qemu_plugin_reg_ctx *ctx,
                                size_t idx);

/**
 * qemu_plugin_reg_size() - query helper for register size in context
 * @ctx: register context
 * @idx: register index
 *
 * Returns: size of register data in bytes
 */
size_t qemu_plugin_reg_size(const struct qemu_plugin_reg_ctx *ctx,
                            size_t idx);

/**
 * qemu_plugin_regs_load() - load data from all registers
 * and store them in context
 * @ctx: register context
 *
 * This call does not require qemu_plugin_reg_ptr()
 * or qemu_plugin_reg_size() to be called again,
 * because the data will be overwritten in the context at the same positions.
 * So it's just an update call
 */
void qemu_plugin_regs_load(struct qemu_plugin_reg_ctx *ctx);

/**
 * typedef qemu_plugin_meminfo_t - opaque memory transaction handle
 *
 * This can be further queried using the qemu_plugin_mem_* query
 * functions.
 */
typedef uint32_t qemu_plugin_meminfo_t;
/** struct qemu_plugin_hwaddr - opaque hw address handle */
struct qemu_plugin_hwaddr;

/**
 * qemu_plugin_mem_size_shift() - get size of access
 * @info: opaque memory transaction handle
 *
 * Returns: size of access in ^2 (0=byte, 1=16bit, 2=32bit etc...)
 */
QEMU_PLUGIN_API
unsigned int qemu_plugin_mem_size_shift(qemu_plugin_meminfo_t info);
/**
 * qemu_plugin_mem_is_sign_extended() - was the access sign extended
 * @info: opaque memory transaction handle
 *
 * Returns: true if it was, otherwise false
 */
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_sign_extended(qemu_plugin_meminfo_t info);
/**
 * qemu_plugin_mem_is_big_endian() - was the access big endian
 * @info: opaque memory transaction handle
 *
 * Returns: true if it was, otherwise false
 */
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_big_endian(qemu_plugin_meminfo_t info);
/**
 * qemu_plugin_mem_is_store() - was the access a store
 * @info: opaque memory transaction handle
 *
 * Returns: true if it was, otherwise false
 */
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_store(qemu_plugin_meminfo_t info);
/**
 * qemu_plugin_mem_is_load() - was the access a load
 * @info: opaque memory transaction handle
 *
 * Returns: true if it was, otherwise false
 */
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_load(qemu_plugin_meminfo_t info);

/**
 * qemu_plugin_get_hwaddr() - return handle for memory operation
 * @info: opaque memory info structure
 * @vaddr: the virtual address of the memory operation
 *
 * For system emulation returns a qemu_plugin_hwaddr handle to query
 * details about the actual physical address backing the virtual
 * address. For linux-user guests it just returns NULL.
 *
 * This handle is *only* valid for the duration of the callback. Any
 * information about the handle should be recovered before the
 * callback returns.
 */
QEMU_PLUGIN_API
struct qemu_plugin_hwaddr *qemu_plugin_get_hwaddr(qemu_plugin_meminfo_t info,
                                                  uint64_t vaddr);

/*
 * The following additional queries can be run on the hwaddr structure to
 * return information about it - namely whether it is for an IO access and the
 * physical address associated with the access.
 */

/**
 * qemu_plugin_hwaddr_is_io() - query whether memory operation is IO
 * @haddr: address handle from qemu_plugin_get_hwaddr()
 *
 * Returns true if the handle's memory operation is to memory-mapped IO, or
 * false if it is to RAM
 */
QEMU_PLUGIN_API
bool qemu_plugin_hwaddr_is_io(const struct qemu_plugin_hwaddr *haddr);

/**
 * qemu_plugin_hwaddr_phys_addr() - query physical address for memory operation
 * @haddr: address handle from qemu_plugin_get_hwaddr()
 *
 * Returns the physical address associated with the memory operation
 *
 * Note that the returned physical address may not be unique if you are dealing
 * with multiple address spaces.
 */
QEMU_PLUGIN_API
uint64_t qemu_plugin_hwaddr_phys_addr(const struct qemu_plugin_hwaddr *haddr);

/*
 * Returns a string representing the device. The string is valid for
 * the lifetime of the plugin.
 */
QEMU_PLUGIN_API
const char *qemu_plugin_hwaddr_device_name(const struct qemu_plugin_hwaddr *h);

/**
 * typedef qemu_plugin_vcpu_mem_cb_t - memory callback function type
 * @vcpu_index: the executing vCPU
 * @info: an opaque handle for further queries about the memory
 * @vaddr: the virtual address of the transaction
 * @userdata: any user data attached to the callback
 */
typedef void (*qemu_plugin_vcpu_mem_cb_t) (unsigned int vcpu_index,
                                           qemu_plugin_meminfo_t info,
                                           uint64_t vaddr,
                                           void *userdata);

/**
 * qemu_plugin_register_vcpu_mem_cb() - register memory access callback
 * @insn: handle for instruction to instrument
 * @cb: callback of type qemu_plugin_vcpu_mem_cb_t
 * @flags: (currently unused) callback flags
 * @rw: monitor reads, writes or both
 * @userdata: opaque pointer for userdata
 *
 * This registers a full callback for every memory access generated by
 * an instruction. If the instruction doesn't access memory no
 * callback will be made.
 *
 * The callback reports the vCPU the access took place on, the virtual
 * address of the access and a handle for further queries. The user
 * can attach some userdata to the callback for additional purposes.
 *
 * Other execution threads will continue to execute during the
 * callback so the plugin is responsible for ensuring it doesn't get
 * confused by making appropriate use of locking if required.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_mem_cb(struct qemu_plugin_insn *insn,
                                      qemu_plugin_vcpu_mem_cb_t cb,
                                      enum qemu_plugin_cb_flags flags,
                                      enum qemu_plugin_mem_rw rw,
                                      void *userdata);

/**
 * qemu_plugin_register_vcpu_mem_inline() - register an inline op to any memory access
 * @insn: handle for instruction to instrument
 * @rw: apply to reads, writes or both
 * @op: the op, of type qemu_plugin_op
 * @ptr: pointer memory for the op
 * @imm: immediate data for @op
 *
 * This registers a inline op every memory access generated by the
 * instruction. This provides for a lightweight but not thread-safe
 * way of counting the number of operations done.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_mem_inline(struct qemu_plugin_insn *insn,
                                          enum qemu_plugin_mem_rw rw,
                                          enum qemu_plugin_op op, void *ptr,
                                          uint64_t imm);



typedef void
(*qemu_plugin_vcpu_syscall_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_index,
                                 int64_t num, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5,
                                 uint64_t a6, uint64_t a7, uint64_t a8);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_syscall_cb(qemu_plugin_id_t id,
                                          qemu_plugin_vcpu_syscall_cb_t cb);

typedef void
(*qemu_plugin_vcpu_syscall_ret_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_idx,
                                     int64_t num, int64_t ret);

QEMU_PLUGIN_API
void
qemu_plugin_register_vcpu_syscall_ret_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_syscall_ret_cb_t cb);


/**
 * qemu_plugin_insn_disas() - return disassembly string for instruction
 * @insn: instruction reference
 *
 * Returns an allocated string containing the disassembly
 */

QEMU_PLUGIN_API
char *qemu_plugin_insn_disas(const struct qemu_plugin_insn *insn);

#define QEMU_PLUGIN_DISAS_SYNTAX_ENABLED
/**
 * qemu_plugin_insn_disas_with_syntax() - return disassembly string
 * for instruction
 * @insn: instruction reference
 * @syntax: syntax style
 *
 * Returns an allocated string containing the disassembly
 */
char *qemu_plugin_insn_disas_with_syntax(const struct qemu_plugin_insn *insn,
                                         enum qemu_plugin_disas_syntax syntax);

/**
 * qemu_plugin_insn_symbol() - best effort symbol lookup
 * @insn: instruction reference
 *
 * Return a static string referring to the symbol. This is dependent
 * on the binary QEMU is running having provided a symbol table.
 */
QEMU_PLUGIN_API
const char *qemu_plugin_insn_symbol(const struct qemu_plugin_insn *insn);

/**
 * qemu_plugin_vcpu_for_each() - iterate over the existing vCPU
 * @id: plugin ID
 * @cb: callback function
 *
 * The @cb function is called once for each existing vCPU.
 *
 * See also: qemu_plugin_register_vcpu_init_cb()
 */
QEMU_PLUGIN_API
void qemu_plugin_vcpu_for_each(qemu_plugin_id_t id,
                               qemu_plugin_vcpu_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_flush_cb(qemu_plugin_id_t id,
                                   qemu_plugin_simple_cb_t cb);

/**
 * qemu_plugin_register_atexit_cb() - register exit callback
 * @id: plugin ID
 * @cb: callback
 * @userdata: user data for callback
 *
 * The @cb function is called once execution has finished. Plugins
 * should be able to free all their resources at this point much like
 * after a reset/uninstall callback is called.
 *
 * In user-mode it is possible a few un-instrumented instructions from
 * child threads may run before the host kernel reaps the threads.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_atexit_cb(qemu_plugin_id_t id,
                                    qemu_plugin_udata_cb_t cb, void *userdata);

/* returns -1 in user-mode */
int qemu_plugin_n_vcpus(void);

/* returns -1 in user-mode */
int qemu_plugin_n_max_vcpus(void);

/**
 * qemu_plugin_outs() - output string via QEMU's logging system
 * @string: a string
 */
QEMU_PLUGIN_API
void qemu_plugin_outs(const char *string);

/**
 * qemu_plugin_bool_parse() - parses a boolean argument in the form of
 * "<argname>=[on|yes|true|off|no|false]"
 *
 * @name: argument name, the part before the equals sign
 * @val: argument value, what's after the equals sign
 * @ret: output return value
 *
 * returns true if the combination @name=@val parses correctly to a boolean
 * argument, and false otherwise
 */
QEMU_PLUGIN_API
bool qemu_plugin_bool_parse(const char *name, const char *val, bool *ret);

/**
 * qemu_plugin_path_to_binary() - path to binary file being executed
 *
 * Return a string representing the path to the binary. For user-mode
 * this is the main executable. For system emulation we currently
 * return NULL. The user should g_free() the string once no longer
 * needed.
 */
QEMU_PLUGIN_API
const char *qemu_plugin_path_to_binary(void);

/**
 * qemu_plugin_start_code() - returns start of text segment
 *
 * Returns the nominal start address of the main text segment in
 * user-mode. Currently returns 0 for system emulation.
 */
QEMU_PLUGIN_API
uint64_t qemu_plugin_start_code(void);

/**
 * qemu_plugin_end_code() - returns end of text segment
 *
 * Returns the nominal end address of the main text segment in
 * user-mode. Currently returns 0 for system emulation.
 */
QEMU_PLUGIN_API
uint64_t qemu_plugin_end_code(void);

/**
 * qemu_plugin_entry_code() - returns start address for module
 *
 * Returns the nominal entry address of the main text segment in
 * user-mode. Currently returns 0 for system emulation.
 */
QEMU_PLUGIN_API
uint64_t qemu_plugin_entry_code(void);


#define QEMU_PLUGIN_READ_PHYS_MEM_ENABLED
/**
 * qemu_plugin_vcpu_read_phys_mem() - reads guest's memory
 *
 * @vcpu_index: vcpu index
 * @addr: hardware address of the guest
 * @buf: host's destination buffer to read data to
 * @len: number of bytes to read
 *
 * Reads memory from the guest by it's hardware address.
 * NOTE: the memory value may not be the same as the value
 * that was read by the instruction. This could because of a few reasons:
 * - an mmio write changes underlying memory layout
 * - another thread changes memory after the access
 */
void qemu_plugin_vcpu_read_phys_mem(unsigned int vcpu_index,
                                    uint64_t addr,
                                    void *buf,
                                    uint64_t len);


#define QEMU_GET_LOG_FILENAME_ENABLED
/**
 * qemu_plugin_get_log_filename() - returns global log filename
 *
 * Returns global QEMU log filename.
 */
const char *qemu_plugin_get_log_filename(void);

#endif /* QEMU_QEMU_PLUGIN_H */
