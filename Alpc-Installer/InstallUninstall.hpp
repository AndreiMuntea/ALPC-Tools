/**
 * @file        ALPC-Tools/Alpc-Installer/InstallUninstall.hpp
 *
 * @brief       In this file we define the methods for aiding us in
 *              installation and uninstalation.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#pragma once

#include "Helpers.hpp"

/**
 * @brief   Performs the required steps for installing the solution.
 *
 * @return  nothing.
 */
void
DoInstall(
    void
) noexcept(true);

/**
 * @brief   Performs the required steps for uninstalling the solution.
 *
 * @return  nothing.
 */
void
DoUninstall(
    void
) noexcept(true);
