//////////////////////////////////////////////// 
// 
// klamath C Defines from Memmap Excel File 
// Source Excel: klamath_memmap.xlsx 
//         Time: Mon Jan 27 17:13:50 2025
//         User: aktambol
// 
//////////////////////////////////////////////// 

#define       START_DRAM_0GBTO2GB                  0x00000000 
#define         END_DRAM_0GBTO2GB                  0x7FFFFFFF 
#define        SIZE_DRAM_0GBTO2GB                  0x80000000 
#define     DEC_BIT_DRAM_0GBTO2GB                  0X1F 

#define       START_DRAM_2GBTO3GB                  0x80000000 
#define         END_DRAM_2GBTO3GB                  0xBFFFFFFF 
#define        SIZE_DRAM_2GBTO3GB                  0x40000000 
#define     DEC_BIT_DRAM_2GBTO3GB                  0X1E 

#define       START_DRAM_3GBTO3P5GB                0xC0000000 
#define         END_DRAM_3GBTO3P5GB                0xDFFFFFFF 
#define        SIZE_DRAM_3GBTO3P5GB                0x20000000 
#define     DEC_BIT_DRAM_3GBTO3P5GB                0X1D 

#define       START_SYSMGR                         0xE0000000 
#define         END_SYSMGR                         0xEFFFFFFF 
#define        SIZE_SYSMGR                         0x10000000 
#define     DEC_BIT_SYSMGR                         0X1C 

#define       START_SPI_FLASH                      0xF0000000 
#define         END_SPI_FLASH                      0xF1FFFFFF 
#define        SIZE_SPI_FLASH                      0x2000000 
#define     DEC_BIT_SPI_FLASH                      0X19 

#define       START_AVIO_VPP128B                   0xF7400000 
#define         END_AVIO_VPP128B                   0xF740FFFF 
#define        SIZE_AVIO_VPP128B                   0x10000 
#define     DEC_BIT_AVIO_VPP128B                   0X10 

#define       START_AVIO_REG                       0xF7400000 
#define         END_AVIO_REG                       0xF75FFFFF 
#define        SIZE_AVIO_REG                       0x200000 
#define     DEC_BIT_AVIO_REG                       0X15 

#define       START_AVIO_LCDC2                     0xF7410000 
#define         END_AVIO_LCDC2                     0xF741FFFF 
#define        SIZE_AVIO_LCDC2                     0x10000 
#define     DEC_BIT_AVIO_LCDC2                     0X10 

#define       START_AVIO_VPP_GBL                   0xF7420000 
#define         END_AVIO_VPP_GBL                   0xF7423FFF 
#define        SIZE_AVIO_VPP_GBL                   0x4000 
#define     DEC_BIT_AVIO_VPP_GBL                   0XE 

#define       START_AVIO_VPP_BCMQ                  0xF7424000 
#define         END_AVIO_VPP_BCMQ                  0xF74243FF 
#define        SIZE_AVIO_VPP_BCMQ                  0x400 
#define     DEC_BIT_AVIO_VPP_BCMQ                  0XA 

#define       START_AVIO_VIP128B_DHUB              0xF7440000 
#define         END_AVIO_VIP128B_DHUB              0xF744FFFF 
#define        SIZE_AVIO_VIP128B_DHUB              0x10000 
#define     DEC_BIT_AVIO_VIP128B_DHUB              0X10 

#define       START_AVIO_CSIHOST                   0xF7450000 
#define         END_AVIO_CSIHOST                   0xF7457FFF 
#define        SIZE_AVIO_CSIHOST                   0x8000 
#define     DEC_BIT_AVIO_CSIHOST                   0XF 

#define       START_AVIO_CSIPIPE                   0xF7458000 
#define         END_AVIO_CSIPIPE                   0xF745FFFF 
#define        SIZE_AVIO_CSIPIPE                   0x8000 
#define     DEC_BIT_AVIO_CSIPIPE                   0XF 

#define       START_AVIO_VIP_GBL                   0xF7460000 
#define         END_AVIO_VIP_GBL                   0xF7463FFF 
#define        SIZE_AVIO_VIP_GBL                   0x4000 
#define     DEC_BIT_AVIO_VIP_GBL                   0XE 

#define       START_AVIO_VIP_BCMQ                  0xF7464000 
#define         END_AVIO_VIP_BCMQ                  0xF74643FF 
#define        SIZE_AVIO_VIP_BCMQ                  0x400 
#define     DEC_BIT_AVIO_VIP_BCMQ                  0XA 

#define       START_AVIO_AIO64B_DHUB               0xF7480000 
#define         END_AVIO_AIO64B_DHUB               0xF748FFFF 
#define        SIZE_AVIO_AIO64B_DHUB               0x10000 
#define     DEC_BIT_AVIO_AIO64B_DHUB               0X10 

#define       START_AVIO_AIO_GBL                   0xF7490000 
#define         END_AVIO_AIO_GBL                   0xF7493FFF 
#define        SIZE_AVIO_AIO_GBL                   0x4000 
#define     DEC_BIT_AVIO_AIO_GBL                   0XE 

#define       START_AVIO_BCM                       0xF7494000 
#define         END_AVIO_BCM                       0xF74943FF 
#define        SIZE_AVIO_BCM                       0x400 
#define     DEC_BIT_AVIO_BCM                       0XA 

#define       START_AVIO_PTRACK1                   0xF7494400 
#define         END_AVIO_PTRACK1                   0xF74947FF 
#define        SIZE_AVIO_PTRACK1                   0x400 
#define     DEC_BIT_AVIO_PTRACK1                   0XA 

#define       START_AVIO_PTRACK2                   0xF7494800 
#define         END_AVIO_PTRACK2                   0xF7494BFF 
#define        SIZE_AVIO_PTRACK2                   0x400 
#define     DEC_BIT_AVIO_PTRACK2                   0XA 

#define       START_AVIO_I2S                       0xF7494C00 
#define         END_AVIO_I2S                       0xF7494FFF 
#define        SIZE_AVIO_I2S                       0x400 
#define     DEC_BIT_AVIO_I2S                       0XA 

#define       START_SYNPU_REG                      0xF7600000 
#define         END_SYNPU_REG                      0xF77FFFFF 
#define        SIZE_SYNPU_REG                      0x200000 
#define     DEC_BIT_SYNPU_REG                      0X15 

#define       START_SMMU_REG                       0xF7800000 
#define         END_SMMU_REG                       0xF787FFFF 
#define        SIZE_SMMU_REG                       0x80000 
#define     DEC_BIT_SMMU_REG                       0X13 

#define       START_GFX3D_REG                      0xF7980000 
#define         END_GFX3D_REG                      0xF7983FFF 
#define        SIZE_GFX3D_REG                      0x4000 
#define     DEC_BIT_GFX3D_REG                      0XE 

#define       START_EMMC_REG                       0xF7A00000 
#define         END_EMMC_REG                       0xF7A00FFF 
#define        SIZE_EMMC_REG                       0x1000 
#define     DEC_BIT_EMMC_REG                       0XC 

#define       START_SDIO0_REG                      0xF7A01000 
#define         END_SDIO0_REG                      0xF7A01FFF 
#define        SIZE_SDIO0_REG                      0x1000 
#define     DEC_BIT_SDIO0_REG                      0XC 

#define       START_SDIO1_REG                      0xF7A02000 
#define         END_SDIO1_REG                      0xF7A02FFF 
#define        SIZE_SDIO1_REG                      0x1000 
#define     DEC_BIT_SDIO1_REG                      0XC 

#define       START_GE0_REG                        0xF7A04000 
#define         END_GE0_REG                        0xF7A05FFF 
#define        SIZE_GE0_REG                        0x2000 
#define     DEC_BIT_GE0_REG                        0XD 

#define       START_GE1_REG                        0xF7A06000 
#define         END_GE1_REG                        0xF7A07FFF 
#define        SIZE_GE1_REG                        0x2000 
#define     DEC_BIT_GE1_REG                        0XD 

#define       START_USB0_REG                       0xF7B00000 
#define         END_USB0_REG                       0xF7B1FFFF 
#define        SIZE_USB0_REG                       0x20000 
#define     DEC_BIT_USB0_REG                       0X11 

#define       START_USB1_REG                       0xF7B20000 
#define         END_USB1_REG                       0xF7B3FFFF 
#define        SIZE_USB1_REG                       0x20000 
#define     DEC_BIT_USB1_REG                       0X11 

#define       START_MC_ALM_REG                     0xF7E00000 
#define         END_MC_ALM_REG                     0xF7E00FFF 
#define        SIZE_MC_ALM_REG                     0x1000 
#define     DEC_BIT_MC_ALM_REG                     0XC 

#define       START_MTEST_REG                      0xF7E01000 
#define         END_MTEST_REG                      0xF7E01FFF 
#define        SIZE_MTEST_REG                      0x1000 
#define     DEC_BIT_MTEST_REG                      0XC 

#define       START_CHIP_CTRL_REG                  0xF7E10000 
#define         END_CHIP_CTRL_REG                  0xF7E1FFFF 
#define        SIZE_CHIP_CTRL_REG                  0x10000 
#define     DEC_BIT_CHIP_CTRL_REG                  0X10 

#define       START_SOC_REG                        0xF7E20000 
#define         END_SOC_REG                        0xF7E21FFF 
#define        SIZE_SOC_REG                        0x2000 
#define     DEC_BIT_SOC_REG                        0XD 

#define       START_IPC_REG                        0xF7E22000 
#define         END_IPC_REG                        0xF7E22FFF 
#define        SIZE_IPC_REG                        0x1000 
#define     DEC_BIT_IPC_REG                        0XC 

#define       START_ACPU_REG                       0xF7E30000 
#define         END_ACPU_REG                       0xF7E33FFF 
#define        SIZE_ACPU_REG                       0x4000 
#define     DEC_BIT_ACPU_REG                       0XE 

#define       START_DDR_REG                        0xF7E40000 
#define         END_DDR_REG                        0xF7E4FFFF 
#define        SIZE_DDR_REG                        0x10000 
#define     DEC_BIT_DDR_REG                        0X10 

#define       START_MCTRLSS_REG                    0xF7E50000 
#define         END_MCTRLSS_REG                    0xF7E50FFF 
#define        SIZE_MCTRLSS_REG                    0x1000 
#define     DEC_BIT_MCTRLSS_REG                    0XC 

#define       START_MC_DFI0_REG                    0xF7E52000 
#define         END_MC_DFI0_REG                    0xF7E53FFF 
#define        SIZE_MC_DFI0_REG                    0x2000 
#define     DEC_BIT_MC_DFI0_REG                    0XD 

#define       START_GIC_REG                        0xF7E58000 
#define         END_GIC_REG                        0xF7E5FFFF 
#define        SIZE_GIC_REG                        0x8000 
#define     DEC_BIT_GIC_REG                        0XF 

#define       START_UART0_REG                      0xF7F00000 
#define         END_UART0_REG                      0xF7F00FFF 
#define        SIZE_UART0_REG                      0x1000 
#define     DEC_BIT_UART0_REG                      0XC 

#define       START_APBPERIF_REG                   0xF7F00000 
#define         END_APBPERIF_REG                   0xF7F0FFFF 
#define        SIZE_APBPERIF_REG                   0x10000 
#define     DEC_BIT_APBPERIF_REG                   0X10 

#define       START_UART1_REG                      0xF7F01000 
#define         END_UART1_REG                      0xF7F01FFF 
#define        SIZE_UART1_REG                      0x1000 
#define     DEC_BIT_UART1_REG                      0XC 

#define       START_UART2_REG                      0xF7F02000 
#define         END_UART2_REG                      0xF7F02FFF 
#define        SIZE_UART2_REG                      0x1000 
#define     DEC_BIT_UART2_REG                      0XC 

#define       START_UART3_REG                      0xF7F03000 
#define         END_UART3_REG                      0xF7F03FFF 
#define        SIZE_UART3_REG                      0x1000 
#define     DEC_BIT_UART3_REG                      0XC 

#define       START_SPI_M0_REG                     0xF7F04000 
#define         END_SPI_M0_REG                     0xF7F04FFF 
#define        SIZE_SPI_M0_REG                     0x1000 
#define     DEC_BIT_SPI_M0_REG                     0XC 

#define       START_I2C0_REG                       0xF7F05000 
#define         END_I2C0_REG                       0xF7F05FFF 
#define        SIZE_I2C0_REG                       0x1000 
#define     DEC_BIT_I2C0_REG                       0XC 

#define       START_I2C1_REG                       0xF7F06000 
#define         END_I2C1_REG                       0xF7F06FFF 
#define        SIZE_I2C1_REG                       0x1000 
#define     DEC_BIT_I2C1_REG                       0XC 

#define       START_GPIO0_REG                      0xF7F07000 
#define         END_GPIO0_REG                      0xF7F07FFF 
#define        SIZE_GPIO0_REG                      0x1000 
#define     DEC_BIT_GPIO0_REG                      0XC 

#define       START_ICTL0_REG                      0xF7F08000 
#define         END_ICTL0_REG                      0xF7F08FFF 
#define        SIZE_ICTL0_REG                      0x1000 
#define     DEC_BIT_ICTL0_REG                      0XC 

#define       START_ICTL1_REG                      0xF7F09000 
#define         END_ICTL1_REG                      0xF7F09FFF 
#define        SIZE_ICTL1_REG                      0x1000 
#define     DEC_BIT_ICTL1_REG                      0XC 

#define       START_ICTL2_REG                      0xF7F0A000 
#define         END_ICTL2_REG                      0xF7F0AFFF 
#define        SIZE_ICTL2_REG                      0x1000 
#define     DEC_BIT_ICTL2_REG                      0XC 

#define       START_SPI_M1_REG                     0xF7F0B000 
#define         END_SPI_M1_REG                     0xF7F0BFFF 
#define        SIZE_SPI_M1_REG                     0x1000 
#define     DEC_BIT_SPI_M1_REG                     0XC 

#define       START_SPI_M2_REG                     0xF7F0C000 
#define         END_SPI_M2_REG                     0xF7F0CFFF 
#define        SIZE_SPI_M2_REG                     0x1000 
#define     DEC_BIT_SPI_M2_REG                     0XC 

#define       START_SPI_M3_REG                     0xF7F0D000 
#define         END_SPI_M3_REG                     0xF7F0DFFF 
#define        SIZE_SPI_M3_REG                     0x1000 
#define     DEC_BIT_SPI_M3_REG                     0XC 

#define       START_GPIO1_REG                      0xF7F0E000 
#define         END_GPIO1_REG                      0xF7F0EFFF 
#define        SIZE_GPIO1_REG                      0x1000 
#define     DEC_BIT_GPIO1_REG                      0XC 

#define       START_APBTIMERS_REG                  0xF7F20000 
#define         END_APBTIMERS_REG                  0xF7F3FFFF 
#define        SIZE_APBTIMERS_REG                  0x20000 
#define     DEC_BIT_APBTIMERS_REG                  0X11 

#define       START_TIMER0_REG                     0xF7F20000 
#define         END_TIMER0_REG                     0xF7F20FFF 
#define        SIZE_TIMER0_REG                     0x1000 
#define     DEC_BIT_TIMER0_REG                     0XC 

#define       START_TIMER1_REG                     0xF7F21000 
#define         END_TIMER1_REG                     0xF7F21FFF 
#define        SIZE_TIMER1_REG                     0x1000 
#define     DEC_BIT_TIMER1_REG                     0XC 

#define       START_TIMER2_REG                     0xF7F22000 
#define         END_TIMER2_REG                     0xF7F22FFF 
#define        SIZE_TIMER2_REG                     0x1000 
#define     DEC_BIT_TIMER2_REG                     0XC 

#define       START_TIMER3_REG                     0xF7F23000 
#define         END_TIMER3_REG                     0xF7F23FFF 
#define        SIZE_TIMER3_REG                     0x1000 
#define     DEC_BIT_TIMER3_REG                     0XC 

#define       START_TIMER4_REG                     0xF7F24000 
#define         END_TIMER4_REG                     0xF7F24FFF 
#define        SIZE_TIMER4_REG                     0x1000 
#define     DEC_BIT_TIMER4_REG                     0XC 

#define       START_TIMER5_REG                     0xF7F25000 
#define         END_TIMER5_REG                     0xF7F25FFF 
#define        SIZE_TIMER5_REG                     0x1000 
#define     DEC_BIT_TIMER5_REG                     0XC 

#define       START_TIMER6_REG                     0xF7F26000 
#define         END_TIMER6_REG                     0xF7F26FFF 
#define        SIZE_TIMER6_REG                     0x1000 
#define     DEC_BIT_TIMER6_REG                     0XC 

#define       START_TIMER7_REG                     0xF7F27000 
#define         END_TIMER7_REG                     0xF7F27FFF 
#define        SIZE_TIMER7_REG                     0x1000 
#define     DEC_BIT_TIMER7_REG                     0XC 

#define       START_TIMER8_REG                     0xF7F28000 
#define         END_TIMER8_REG                     0xF7F28FFF 
#define        SIZE_TIMER8_REG                     0x1000 
#define     DEC_BIT_TIMER8_REG                     0XC 

#define       START_TIMER9_REG                     0xF7F29000 
#define         END_TIMER9_REG                     0xF7F29FFF 
#define        SIZE_TIMER9_REG                     0x1000 
#define     DEC_BIT_TIMER9_REG                     0XC 

#define       START_TIMER10_REG                    0xF7F2A000 
#define         END_TIMER10_REG                    0xF7F2AFFF 
#define        SIZE_TIMER10_REG                    0x1000 
#define     DEC_BIT_TIMER10_REG                    0XC 

#define       START_TIMER11_REG                    0xF7F2B000 
#define         END_TIMER11_REG                    0xF7F2BFFF 
#define        SIZE_TIMER11_REG                    0x1000 
#define     DEC_BIT_TIMER11_REG                    0XC 

#define       START_TIMER12_REG                    0xF7F2C000 
#define         END_TIMER12_REG                    0xF7F2CFFF 
#define        SIZE_TIMER12_REG                    0x1000 
#define     DEC_BIT_TIMER12_REG                    0XC 

#define       START_TIMER13_REG                    0xF7F2D000 
#define         END_TIMER13_REG                    0xF7F2DFFF 
#define        SIZE_TIMER13_REG                    0x1000 
#define     DEC_BIT_TIMER13_REG                    0XC 

#define       START_TIMER14_REG                    0xF7F2E000 
#define         END_TIMER14_REG                    0xF7F2EFFF 
#define        SIZE_TIMER14_REG                    0x1000 
#define     DEC_BIT_TIMER14_REG                    0XC 

#define       START_TIMER15_REG                    0xF7F2F000 
#define         END_TIMER15_REG                    0xF7F2FFFF 
#define        SIZE_TIMER15_REG                    0x1000 
#define     DEC_BIT_TIMER15_REG                    0XC 

#define       START_WD0_REG                        0xF7F30000 
#define         END_WD0_REG                        0xF7F31FFF 
#define        SIZE_WD0_REG                        0x2000 
#define     DEC_BIT_WD0_REG                        0XD 

#define       START_WD1_REG                        0xF7F32000 
#define         END_WD1_REG                        0xF7F33FFF 
#define        SIZE_WD1_REG                        0x2000 
#define     DEC_BIT_WD1_REG                        0XD 

#define       START_WD2_REG                        0xF7F34000 
#define         END_WD2_REG                        0xF7F35FFF 
#define        SIZE_WD2_REG                        0x2000 
#define     DEC_BIT_WD2_REG                        0XD 

#define       START_SYSCOUNT_CTRL_REG              0xF7F3E000 
#define         END_SYSCOUNT_CTRL_REG              0xF7F3EFFF 
#define        SIZE_SYSCOUNT_CTRL_REG              0x1000 
#define     DEC_BIT_SYSCOUNT_CTRL_REG              0XC 

#define       START_SYSCOUNT_FRM_REG               0xF7F3F000 
#define         END_SYSCOUNT_FRM_REG               0xF7F3FFFF 
#define        SIZE_SYSCOUNT_FRM_REG               0x1000 
#define     DEC_BIT_SYSCOUNT_FRM_REG               0XC 

#define       START_PDMA_REG                       0xF7F40000 
#define         END_PDMA_REG                       0xF7F43FFF 
#define        SIZE_PDMA_REG                       0x4000 
#define     DEC_BIT_PDMA_REG                       0XE 

#define       START_MPTS_REG                       0xF9000000 
#define         END_MPTS_REG                       0xF903FFFF 
#define        SIZE_MPTS_REG                       0x40000 
#define     DEC_BIT_MPTS_REG                       0X12 

#define       START_HIGH_ROM_VECTOR                0xFFFF0000 
#define         END_HIGH_ROM_VECTOR                0xFFFFFFFF 
#define        SIZE_HIGH_ROM_VECTOR                0x10000 
#define     DEC_BIT_HIGH_ROM_VECTOR                0X10 

#define MEMMAP_CHIP_CTRL_REG_BASE START_CHIP_CTRL_REG
#define Gbl_bootStrap_bootSrc_ROM_SPI_BOOT Gbl_bootStrap_bootSrc_ROM_BOOT_FROM_XSPI
#define MEMMAP_CA7_REG_BASE START_ACPU_REG
