/*
 * extra-ksyms.c
 *
 * Extra kernel symbol support for dynamic linking.
 */
#include <linux/module.h>

/* CONFIG_KALLSYMS is mandatory requirement for this mechanism */
#ifndef CONFIG_KALLSYMS
#error Linux kernel must be built with CONFIG_KALLSYMS
#endif

/* nvidia_p2p_get_pages */
static struct module *mod_nvidia_p2p_get_pages = NULL;
static int (* p_nvidia_p2p_get_pages)(
	uint64_t p2p_token,
	uint32_t va_space,
	uint64_t virtual_address,
	uint64_t length,
	struct nvidia_p2p_page_table **p_table,
	void (*free_callback)(void *data),
	void *data) = NULL;

inline int
__nvidia_p2p_get_pages(uint64_t p2p_token,
					   uint32_t va_space,
					   uint64_t virtual_address,
					   uint64_t length,
					   struct nvidia_p2p_page_table **page_table,
					   void (*free_callback)(void *data),
					   void *data)
{
	if (unlikely(!p_nvidia_p2p_get_pages))
		return -EINVAL;
	return p_nvidia_p2p_get_pages(p2p_token,
								  va_space,
								  virtual_address,
								  length,
								  page_table,
								  free_callback,
								  data);
}

/* nvidia_p2p_put_pages */
static struct module *mod_nvidia_p2p_put_pages = NULL;
static int (* p_nvidia_p2p_put_pages)(
	uint64_t p2p_token,
	uint32_t va_space,
	uint64_t virtual_address,
	struct nvidia_p2p_page_table *p_table) = NULL;

static inline int
__nvidia_p2p_put_pages(uint64_t p2p_token, 
					   uint32_t va_space,
					   uint64_t virtual_address,
					   struct nvidia_p2p_page_table *page_table)
{
	if (unlikely(!p_nvidia_p2p_put_pages))
		return -EINVAL;
	return p_nvidia_p2p_put_pages(p2p_token,
								  va_space,
								  virtual_address,
								  page_table);
}

/* nvidia_p2p_free_page_table */
static struct module *mod_nvidia_p2p_free_page_table = NULL;
static int (*p_nvidia_p2p_free_page_table)(
	struct nvidia_p2p_page_table *page_table) = NULL;

static inline int
__nvidia_p2p_free_page_table(struct nvidia_p2p_page_table *page_table)
{
	if (unlikely(!p_nvidia_p2p_free_page_table))
		return -EINVAL;
	return p_nvidia_p2p_free_page_table(page_table);
}

/* ext4_get_block */
static struct module *mod_ext4_get_block = NULL;
static int (* p_ext4_get_block)(
	struct inode *inode, sector_t offset,
	struct buffer_head *bh, int create) = NULL;

static inline int
__ext4_get_block(struct inode *inode, sector_t offset,
				 struct buffer_head *bh, int create)
{
	if (unlikely(!p_ext4_get_block))
		return -EINVAL;
	return p_ext4_get_block(inode, offset, bh, create);
}

/* xfs_get_blocks */
static struct module *mod_xfs_get_blocks = NULL;
static int (* p_xfs_get_blocks)(
	struct inode *inode, sector_t offset,
	struct buffer_head *bh, int create) = NULL;

static inline int
__xfs_get_blocks(struct inode *inode, sector_t offset,
				 struct buffer_head *bh, int create)
{
	if (unlikely(!p_xfs_get_blocks))
		return -EINVAL;
	return p_xfs_get_blocks(inode, offset, bh, create);
}

#ifdef STROM_TARGET_KERNEL_RHEL7
/* nvme_setup_prps */
static struct module *mod_nvme_setup_prps = NULL;
static int (* p_nvme_setup_prps)(struct nvme_dev *dev,
								 struct nvme_iod *iod,
								 int total_len, gfp_t gfp) = NULL;
static inline int
__nvme_setup_prps(struct nvme_dev *dev,
				  struct nvme_iod *iod,
				  int total_len, gfp_t gfp)
{
	if (unlikely(!p_nvme_setup_prps))
		return -EINVAL;
	return p_nvme_setup_prps(dev, iod, total_len, gfp);
}

/* nvme_submit_io_cmd */
static struct module *mod_nvme_submit_io_cmd = NULL;
static int (* p_nvme_submit_io_cmd)(struct nvme_dev *dev,
									struct nvme_ns *ns,
									struct nvme_command *cmd,
									u32 *result) = NULL;
static inline int
__nvme_submit_io_cmd(struct nvme_dev *dev,
					 struct nvme_ns *ns,
					 struct nvme_command *cmd,
					 u32 *result)
{
	if (unlikely(!p_nvme_submit_io_cmd))
		return -EINVAL;
	return p_nvme_submit_io_cmd(dev, ns, cmd, result);
}
#endif

/*
 * __strom_lookup_extra_symbol - lookup extra symbol and grab module if any
 */
static inline int
__strom_lookup_extra_symbol(const char *symbol_name,
							void **symbol_addr,
							struct module **symbol_mod)
{
	unsigned long	addr;
	struct module  *mod;

	addr = kallsyms_lookup_name(symbol_name);
	if (!addr)
	{
		printk(KERN_ERR NVME_STROM_PREFIX
			   "could not solve the kernel symbol: %s\n",
			   symbol_name);
		return -ENOENT;
	}

	mod = __module_text_address(addr);
	if (mod)
	{
		__module_get(mod);
		printk(KERN_NOTICE NVME_STROM_PREFIX
			   "extra symbol \"%s\" found at %p of module \"%s\"\n",
			   symbol_name, (void *)addr, mod->name);
	}
	else
	{
		printk(KERN_NOTICE NVME_STROM_PREFIX
			   "extra symbol \"%s\" found at %p (core kernel)\n",
			   symbol_name, (void *)addr);
	}
	*symbol_addr = (void *) addr;
	*symbol_mod  = mod;

	return 0;
}

/*
 * strom_update_extra_symbols
 *
 * It solves optional symbols and grab the module that provides these
 * symbols. If we call 
 */
static int
strom_update_extra_symbols(struct notifier_block *nb,
						   unsigned long action, void *data)
{
#define LOOKUP_OPTIONAL_EXTRA_SYMBOL(SYMBOL)					\
	do {														\
		if (!p_##SYMBOL)										\
			__strom_lookup_extra_symbol(#SYMBOL,				\
										(void **)&p_##SYMBOL,	\
										&mod_##SYMBOL);			\
	} while(0)

	/* ext4 */
	LOOKUP_OPTIONAL_EXTRA_SYMBOL(ext4_get_block);
	/* xfs */
	LOOKUP_OPTIONAL_EXTRA_SYMBOL(xfs_get_blocks);

	return 0;
#undef LOOKUP_OPTIONAL_EXTRA_SYMBOL
}

/* notifier of extra symbols */
static struct notifier_block nvme_strom_nb = {
	.notifier_call	= strom_update_extra_symbols
};

/* put all the modules that own the extra symbols */
static inline void
strom_put_all_extra_modules(void)
{
	module_put(mod_nvidia_p2p_get_pages);
	module_put(mod_nvidia_p2p_put_pages);
	module_put(mod_nvidia_p2p_free_page_table);
#ifdef STROM_TARGET_KERNEL_RHEL7
	module_put(mod_nvme_setup_prps);
	module_put(mod_nvme_submit_io_cmd);
#endif
	module_put(mod_ext4_get_block);
	module_put(mod_xfs_get_blocks);
}

/*
 * strom_cleanup_extra_symbols - release modules where the extra symbol
 * points to, and unregister the notifier.
 */
static void __exit
strom_exit_extra_symbols(void)
{
	unregister_module_notifier(&nvme_strom_nb);
	strom_put_all_extra_modules();
}

/*
 * strom_init_extra_symbols - solve the mandatory symbols and grab the module
 */
static int __init
strom_init_extra_symbols(void)
{
	int		rc;

#define LOOKUP_MANDATORY_EXTRA_SYMBOL(SYMBOL)					\
	do {														\
		rc = __strom_lookup_extra_symbol(#SYMBOL,				\
										 (void **)&p_##SYMBOL,	\
										 &mod_##SYMBOL);		\
		if (rc)													\
			goto cleanup;										\
	} while(0)

	/* nvidia.ko */
	LOOKUP_MANDATORY_EXTRA_SYMBOL(nvidia_p2p_get_pages);
	LOOKUP_MANDATORY_EXTRA_SYMBOL(nvidia_p2p_put_pages);
	LOOKUP_MANDATORY_EXTRA_SYMBOL(nvidia_p2p_free_page_table);
#ifdef STROM_TARGET_KERNEL_RHEL7
	/* nvme.ko */
	LOOKUP_MANDATORY_EXTRA_SYMBOL(nvme_setup_prps);
	LOOKUP_MANDATORY_EXTRA_SYMBOL(nvme_submit_io_cmd);
#endif

	/* notifier to get optional extra symbols */
	rc = register_module_notifier(&nvme_strom_nb);
	if (rc)
		goto cleanup;

	return 0;

cleanup:
	strom_put_all_extra_modules();
	return rc;
}

#undef LOOKUP_OPTIONAL_EXTRA_SYMBOL
#undef LOOKUP_MANDATORY_EXTRA_SYMBOL