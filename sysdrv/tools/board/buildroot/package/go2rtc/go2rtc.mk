################################################################################
#
# go2rtc
#
################################################################################

GO2RTC_VERSION = v1.9.14
GO2RTC_SITE = $(call github,AlexxIT,go2rtc,$(GO2RTC_VERSION))
GO2RTC_LICENSE = MIT
GO2RTC_LICENSE_FILES = LICENSE

# Налаштування для Go: видаляємо налагоджувальну інфо для зменшення розміру
GO2RTC_LDFLAGS = -s -w

# GO2RTC_GO_ENV += CGO_ENABLED=1 \
# 	PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)"

# GO2RTC_GO_ENV += CGO_CFLAGS="-I$(STAGING_DIR)/usr/include"
# GO2RTC_GO_ENV += CGO_LDFLAGS="-L$(STAGING_DIR)/usr/lib"

# GO2RTC_GO_ENV = \
# 	CGO_ENABLED=1 \
# 	PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)" \
# 	CC="$(TARGET_CC)" \
# 	CXX="$(TARGET_CXX)" \
# 	CGO_CFLAGS="$(TARGET_CFLAGS) --sysroot=$(STAGING_DIR)" \
# 	CGO_LDFLAGS="$(TARGET_LDFLAGS) --sysroot=$(STAGING_DIR)"

# Важливо: додаємо host-go у залежності, щоб $(HOST_GO_BIN) був доступний
GO2RTC_DEPENDENCIES += host-go host-pkgconf alsa-lib libv4l

# Використовуємо інше ім'я для макросу (наприклад, GO2RTC_VENDOR_MOD),
# щоб уникнути конфлікту з GO2RTC_DOWNLOAD_DEPENDENCIES
define GO2RTC_VENDOR_MOD
	cd $(@D) && \
	GOCACHE=$(HOST_GO_HOST_CACHE) \
	GOPROXY=https://proxy.golang.org,direct \
	$(HOST_DIR)/bin/go mod vendor
endef

# Викликаємо цей макрос після розпакування архіву
GO2RTC_POST_EXTRACT_HOOKS += GO2RTC_VENDOR_MOD

# Використовуємо інфраструктуру golang-package
$(eval $(golang-package))