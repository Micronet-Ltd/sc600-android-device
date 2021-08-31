PRODUCT_RB_OTA	:= $(shell echo $${PRODUCT_RB_OTA})
PRODUCT_PACKAGES += mcu_ua
ifeq ($(PRODUCT_RB_OTA), enabled)
PRODUCT_PACKAGES += libsmm \
		    com.redbend.client \
		    libcrypto_1_1 \
		    libssl_1_1
endif
