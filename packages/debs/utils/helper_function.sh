#!/bin/bash

# DEB helper functions

# Wazuh package builder
# Copyright (C) 2015, Wazuh Inc.
#
# This program is a free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public
# License (version 2) as published by the FSF - Free Software
# Foundation.


setup_build(){
    sources_dir="$1"
    specs_path="$2"
    build_dir="$3"
    package_name="$4"
    debug="$5"

    cp -pr ${specs_path}/wazuh-${BUILD_TARGET}/debian ${sources_dir}/debian
    cp -p /tmp/gen_permissions.sh ${sources_dir}

    # Generating directory structure to build the .deb package
    cd ${build_dir}/${BUILD_TARGET} && tar -czf ${package_name}.orig.tar.gz "${package_name}"

    # Configure the package with the different parameters
    sed -i "s:RELEASE:${PACKAGE_RELEASE}:g" ${sources_dir}/debian/changelog
    sed -i "s:export JOBS=.*:export JOBS=${JOBS}:g" ${sources_dir}/debian/rules
    sed -i "s:export DEBUG_ENABLED=.*:export DEBUG_ENABLED=${debug}:g" ${sources_dir}/debian/rules
    sed -i "s#export PATH=.*#export PATH=/usr/local/gcc-5.5.0/bin:${PATH}#g" ${sources_dir}/debian/rules
    sed -i "s#export LD_LIBRARY_PATH=.*#export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}#g" ${sources_dir}/debian/rules
    sed -i "s:export INSTALLATION_DIR=.*:export INSTALLATION_DIR=${INSTALLATION_PATH}:g" ${sources_dir}/debian/rules
    sed -i "s:DIR=\"/var/ossec\":DIR=\"${INSTALLATION_PATH}\":g" ${sources_dir}/debian/{preinst,postinst,prerm,postrm}
}

set_debug(){
    local debug="$1"
    local sources_dir="$2"
    if [[ "${debug}" == "yes" ]]; then
        sed -i "s:dh_strip --no-automatic-dbgsym::g" ${sources_dir}/debian/rules
    fi
}

build_deps(){
    mk-build-deps -ir -t "apt-get -o Debug::pkgProblemResolver=yes -y"
}

build_package(){

    if [[ "${ARCHITECTURE_TARGET}" == "amd64" ]] ||  [[ "${ARCHITECTURE_TARGET}" == "ppc64le" ]] || \
        [[ "${ARCHITECTURE_TARGET}" == "arm64" ]]; then
        debuild --rootcmd=sudo -b -uc -us
    elif [[ "${ARCHITECTURE_TARGET}" == "armhf" ]]; then
        linux32 debuild --rootcmd=sudo -b -uc -us
    else
        linux32 debuild --rootcmd=sudo -ai386 -b -uc -us
    fi
}

get_checksum(){
    wazuh_version="$1"
    short_commit_hash="$2"
    # TODO: this could be improve and make it in common code inside of build.sh
    deb_file="wazuh-${BUILD_TARGET}_${wazuh_version}-${PACKAGE_RELEASE}"
    if [[ "${ARCHITECTURE_TARGET}" == "ppc64le" ]]; then
        rename="${deb_file}_ppc64el_${short_commit_hash}.deb"
        deb_file="${deb_file}_ppc64el.deb"
    else
        rename="${deb_file}_${ARCHITECTURE_TARGET}_${short_commit_hash}.deb"
        deb_file="${deb_file}_${ARCHITECTURE_TARGET}.deb"
    fi
    pkg_path="${build_dir}/${BUILD_TARGET}"

    if [[ "${checksum}" == "yes" ]]; then
        cd ${pkg_path} && sha512sum ${deb_file} > /var/local/checksum/${deb_file}.sha512
    fi
    mv ${pkg_path}/${deb_file} /var/local/wazuh/${rename}
}