CFG_TEE_TA_LOG_LEVEL ?= 4
CPPFLAGS += -DCFG_TEE_TA_LOG_LEVEL=$(CFG_TEE_TA_LOG_LEVEL) 
TA_DEV_KIT_DIR := /home/avees/Linux_for_Tegra/sources/tegra/optee-src/nv-optee/optee/build/t234/export-ta_arm64/
# The UUID for the Trusted Application
BINARY=7fc5c039-0542-4ee1-80af-b4eab2f1998d

-include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk
# 모든 -O 옵션 제거
cflags-remove-y += -O2 -Os -O3

# 최적화 끄기 + 디버그 심볼
cflags-y += -O0 -g3 -fno-omit-frame-pointer

ifeq ($(wildcard $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk), )
clean:
	@echo 'Note: $$(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk not found, cannot clean TA'
	@echo 'Note: TA_DEV_KIT_DIR=$(TA_DEV_KIT_DIR)'
endif
