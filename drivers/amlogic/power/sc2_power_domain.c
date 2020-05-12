// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/amlogic/pwr_ctrl.h>
#include <dt-bindings/power/sc2-pd.h>

#define DSP_INDEX		7
#define HCODEC_INDEX		8
#define HEVC_INDEX		9
#define VDEC_INDEX		10
#define WAVE_INDEX		11
#define VPU_INDEX		12
#define USB_INDEX		14
#define PCIE_INDEX		15
#define GE2D_INDEX		16
#define ETH_INDEX		23

struct sc2_pm_domain {
	struct generic_pm_domain base;
	int pd_index;
	bool pd_status;
};

static inline struct sc2_pm_domain *
to_sc2_pm_domain(struct generic_pm_domain *genpd)
{
	return container_of(genpd, struct sc2_pm_domain, base);
}

static int sc2_pm_domain_power_off(struct generic_pm_domain *genpd)
{
	struct sc2_pm_domain *pd = to_sc2_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->pd_index, PWR_OFF);

	return 0;
}

static int sc2_pm_domain_power_on(struct generic_pm_domain *genpd)
{
	struct sc2_pm_domain *pd = to_sc2_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->pd_index, PWR_ON);

	return 0;
}

#define POWER_DOMAIN(_name, index, status, flag)		\
struct sc2_pm_domain _name = {					\
		.base = {					\
			.name = #_name,				\
			.power_off = sc2_pm_domain_power_off,	\
			.power_on = sc2_pm_domain_power_on,	\
			.flags = flag, \
		},						\
		.pd_index = index,				\
		.pd_status = status,				\
}

static POWER_DOMAIN(dsp, DSP_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(hcodec, HCODEC_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(hevc, HEVC_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(vdec, VDEC_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(wave, WAVE_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(vpu, VPU_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(usb, USB_INDEX, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(pcie, PCIE_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(ge2d, GE2D_INDEX, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(eth, ETH_INDEX, DOMAIN_INIT_ON, 0);

static struct generic_pm_domain *sc2_onecell_domains[] = {
		[PDID_DSP]			= &dsp.base,
		[PDID_DOS_HCODEC]		= &hcodec.base,
		[PDID_DOS_HEVC]			= &hevc.base,
		[PDID_DOS_VDEC]			= &vdec.base,
		[PDID_DOS_WAVE]			= &wave.base,
		[PDID_VPU_HDMI]			= &vpu.base,
		[PDID_USB_COMB]			= &usb.base,
		[PDID_PCIE]			= &pcie.base,
		[PDID_GE2D]			= &ge2d.base,
		[PDID_ETH]			= &eth.base,
};

static struct genpd_onecell_data sc2_pd_onecell_data = {
	.domains = sc2_onecell_domains,
	.num_domains = ARRAY_SIZE(sc2_onecell_domains),
};

static int sc2_pd_probe(struct platform_device *pdev)
{
	int ret, i;
	struct sc2_pm_domain *pd;

	for (i = 0; i < sc2_pd_onecell_data.num_domains; i++) {
		/* array might be sparse */
		if (!sc2_pd_onecell_data.domains[i])
			continue;

		/* Initialize based on pd_status */
		pd = to_sc2_pm_domain(sc2_pd_onecell_data.domains[i]);
		pm_genpd_init(sc2_pd_onecell_data.domains[i],
			      NULL, pd->pd_status);
	}

	pd_dev_create_file(&pdev->dev, PDID_DSP, PDID_MAX,
			   sc2_onecell_domains);

	ret = of_genpd_add_provider_onecell(pdev->dev.of_node,
					    &sc2_pd_onecell_data);

	if (ret)
		goto out;

	return 0;

out:
	pd_dev_remove_file(&pdev->dev);
	return ret;
}

static const struct of_device_id pd_match_table[] = {
	{ .compatible = "amlogic,sc2-power-domain" },
	{}
};

static struct platform_driver sc2_pd_driver = {
	.probe		= sc2_pd_probe,
	.driver		= {
		.name	= "sc2_pd",
		.of_match_table = pd_match_table,
	},
};

static int sc2_pd_init(void)
{
	return platform_driver_register(&sc2_pd_driver);
}
arch_initcall_sync(sc2_pd_init)

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic Power domain driver");
MODULE_LICENSE("GPL");
