/* QEMU plugin demonstrating code coverage.  Each block of code being 
   executed is listed alongside with the contents of the block itself.

Code Block executions: 7 (# of instructions: 2)
--- 0x40018e217c: "testq %r14, %r14"
--- 0x40018e217f: "je 0x40018e2223"

Code Block executions: 4 (# of instructions: 5)
--- 0x40018e2185: "movq 0xd8(%r15), %rax"
--- 0x40018e218c: "movq %rax, %rdx"
--- 0x40018e218f: "subq %r12, %rdx"
--- 0x40018e2192: "cmpq %rdx, %rbp"
--- 0x40018e2195: "jbe 0x40018e21f0"

Credits: https://qemu.readthedocs.io/en/latest/devel/tcg-plugins.html

Copyright (C) 2021, Steven Wirsz <swirsz@gmail.com>

License: GNU GPL, version 2 or later.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static GMutex lock;
static GHashTable *table;

typedef struct {
    uint64_t addr;
    uint64_t exec_count;
    unsigned long insns;
    gchar   *instset;  
} BlockStruct;

static bool duplicate;

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb);
static void plugin_exit(qemu_plugin_id_t id, void *p);
static void vcpu_tb_exec(unsigned int cpu_index, void *udata);

static int sortfunction(gconstpointer a, gconstpointer b)
{
    BlockStruct *blk_a = (BlockStruct *) a;
    BlockStruct *blk_b = (BlockStruct *) b;
    return blk_a->addr - blk_b->addr;
}
QEMU_PLUGIN_EXPORT

int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
        g_autofree char **tokens = g_strsplit(argv[i], "=", 2);
        qemu_plugin_bool_parse(tokens[0], tokens[1], &duplicate);
    }

    table = g_hash_table_new(NULL, g_direct_equal);

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_autoptr(GString) report = g_string_new("collected ");
    GList *addr, *it;
    int i;

    g_mutex_lock(&lock);
    addr = g_hash_table_get_values(table);
    it = g_list_sort(addr, sortfunction);

    if (it) {
        for (i = 0; it->next; i++, it = it->next) {
            BlockStruct *blk = (BlockStruct *) it->data;
            g_string_append_printf(report, "Code Block executions: %ld (# of instructions: %ld)\n%s\n",
                    blk->exec_count, blk->insns, blk->instset);
        }
        g_list_free(it);
        g_mutex_unlock(&lock);
    }

    g_string_append_printf(report, "Total # of blocks: %d\n", g_hash_table_size(table));
    qemu_plugin_outs(report->str);
}


static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    BlockStruct *blk;
    uint64_t pc = qemu_plugin_tb_vaddr(tb);
    size_t insns = qemu_plugin_tb_n_insns(tb);
    uint64_t hash = pc ^ insns;

    struct qemu_plugin_insn *insn;
    gchar *insn_disas;
    uint64_t insn_vaddr;

    g_mutex_lock(&lock);
    blk = (BlockStruct *) g_hash_table_lookup(table, (gconstpointer) hash);

    blk = g_new0(BlockStruct, 1);
    blk->addr = pc;
    blk->insns = insns;

    size_t n = qemu_plugin_tb_n_insns(tb);
    for (size_t i = 0; i < n; i++) {
        insn = qemu_plugin_tb_get_insn(tb, i);
        insn_vaddr = qemu_plugin_insn_vaddr(insn);
        insn_disas = qemu_plugin_insn_disas(insn);
        gchar* temp = g_strdup_printf("--- 0x%"PRIx64": \"%s\"\n",
            insn_vaddr, insn_disas);
        if (blk->instset != NULL) {
            blk->instset = g_strconcat(blk->instset, temp, NULL);
        } else {
            blk->instset = g_strconcat(temp, NULL);
        }
    }
    g_hash_table_insert(table, (gpointer) hash, (gpointer) blk);
    g_mutex_unlock(&lock);

    if (duplicate) {
        qemu_plugin_register_vcpu_tb_exec_inline(tb, QEMU_PLUGIN_INLINE_ADD_U64,
                                                 &blk->exec_count, 1);
    } else {
        qemu_plugin_register_vcpu_tb_exec_cb(tb, vcpu_tb_exec,
                                             QEMU_PLUGIN_CB_NO_REGS,
                                             (void *)hash);
    }
}
static void vcpu_tb_exec(unsigned int cpu_index, void *udata)
{
    BlockStruct *blk;
    uint64_t hash = (uint64_t) udata;

    g_mutex_lock(&lock);
    blk = (BlockStruct *) g_hash_table_lookup(table, (gconstpointer) hash);
    blk->exec_count++;
    g_mutex_unlock(&lock);
}
