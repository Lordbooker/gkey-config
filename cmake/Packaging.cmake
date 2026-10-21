# Packages (.deb / .rpm) via CPack. The release workflow builds them in
# containers of the target distributions with packaging/build-package.sh.
#
#   cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   (cd build && cpack -G DEB)   # or -G RPM
#
# Options:
#   GKEY_PACKAGE_VERSION  package version, e.g. 1.0.0 (default: from GKEY_VERSION)
#   GKEY_PACKAGE_RELEASE  package release, e.g. "1~ubuntu26.04" (default: 1)

set(CPACK_PACKAGE_NAME "gkey-config")
set(CPACK_PACKAGE_VENDOR "Lordbooker")
set(CPACK_PACKAGE_CONTACT "Lordbooker <lordbooker@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Configuration tool for the gaming keys of Logitech G keyboards (hid-lg-g15)")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Lordbooker/gkey-config")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Package version: numeric part of GKEY_VERSION (e.g. 1.0.0 from 1.0.0-3-gd1568bb).
if(NOT DEFINED GKEY_PACKAGE_VERSION)
    string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" GKEY_PACKAGE_VERSION "${GKEY_VERSION}")
    if(NOT GKEY_PACKAGE_VERSION)
        set(GKEY_PACKAGE_VERSION "${PROJECT_VERSION}")
    endif()
endif()
if(NOT DEFINED GKEY_PACKAGE_RELEASE)
    set(GKEY_PACKAGE_RELEASE "1")
endif()
set(CPACK_PACKAGE_VERSION "${GKEY_PACKAGE_VERSION}")

set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(GKEY_PACKAGING_DIR "${CMAKE_SOURCE_DIR}/packaging")

# --- Debian / Ubuntu ------------------------------------------------------------
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE "${GKEY_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "systemd, udev")
# Both drive the G13; the old libusb driver detaches the kernel driver.
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "linux-g13-driver")
# Qt's own translations (standard buttons such as "Cancel") in the system language.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-translations-l10n")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${GKEY_PACKAGING_DIR}/deb/postinst"
    "${GKEY_PACKAGING_DIR}/deb/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

# --- Fedora ---------------------------------------------------------------------
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE "${GKEY_PACKAGE_RELEASE}")
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)   # appends %{?dist}, e.g. .fc44
set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-only")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_REQUIRES "systemd, systemd-udev")
set(CPACK_RPM_PACKAGE_CONFLICTS "linux-g13-driver")
set(CPACK_RPM_PACKAGE_SUGGESTS "qt6-qttranslations")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE   "${GKEY_PACKAGING_DIR}/rpm/post")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${GKEY_PACKAGING_DIR}/rpm/postun")
# Directories owned by the base system must not be claimed by the package.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/lib/udev
    /usr/lib/udev/rules.d
    /usr/lib/sysusers.d
    /usr/lib/systemd
    /usr/lib/systemd/user
    /usr/share/dbus-1
    /usr/share/dbus-1/services
    /usr/share/applications
    /usr/share/icons
    /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps
    /etc/xdg
    /etc/xdg/autostart
    /usr/share/doc)

include(CPack)
