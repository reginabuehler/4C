// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_CONTACT_DEFINES_HPP
#define FOUR_C_CONTACT_DEFINES_HPP

#include "4C_config.hpp"

FOUR_C_NAMESPACE_OPEN

/************************************************************************/
/* Contact algorithm options                                            */
/************************************************************************/

// #define CONTACTPSEUDO2D           /* flag for pseudo-3D in 2D situations (u_z=0)*/
// #define CONSISTENTSTICK           /* flag for consistent stick branch of the complementarity
//  function */ #define CONSISTENTSLIP            /* flag for consistent slip branch of the
//  complementarity function */

/************************************************************************/
/* Debugging options                                                    */
/************************************************************************/

// GENERAL DEBUGGING OPTIONS

// #define CONTACTSTATUS             /* flag for contact segmentation measurement */
// #define CONTACTFDNORMAL           /* flag for FD check of normal derivative */
// #define CONTACTFDMORTARD          /* flag for FD check of mortar matrix D */
// #define CONTACTFDMORTARM          /* flag for FD check of mortar matrix M */
// #define CONTACTFDGAP              /* flag for FD check of weighted gap g */
// #define CONTACTFDGAPLTL           /* flag for FD check of vector valued gap g ltl */
// #define CONTACTFDJUMPLTL          /* flag for FD check of vector valued jump ltl */
// #define CONTACTFDALPHA            /* flag for FD check of scaling factor alpha*/
// #define CONTACTFDSLIPINCR         /* flag for FD check of weighted obj.-var. slip */

// LAGRANGE DEBUGGING OPTIONS
// #define CONTACTFDTANGLM           /* flag for FD check of (frictionless) tangential LM */
// #define CONTACTFDSTICK            /* flag for FD check of (frictional) stick condition */
// #define CONTACTFDSLIP             /* flag for FD check of (frictional) slip condition */

// PENALTY DEBUGGING OPTIONS
// #define CONTACTFDPENALTYTRAC      /* flag for FD check of penalty tractions */

// IMPLICIT WEAR DEBUGGING OPTIONS
// #define WEARIMPLICITFD            /* flag for FD-check w.r.t. disp. */
// #define WEARIMPLICITFDLM          /* flag for FD-check w.r.t. lm-mult. */

// DISCRETE WEAR DEBUGGING OPTIONS
// #define CONTACTFDT_D              /* flag for FD check of mortar matrix T lin for wear cond */
// #define CONTACTFDE_D              /* flag for FD check of mortar matrix E lin for wear cond */
// #define CONTACTFDMORTART          /* flag for FD check of mortar matrix T */
// #define CONTACTFDMORTARE          /* flag for FD check of mortar matrix E */
// #define CONTACTFDT_D_MASTER       /* flag for FD check of mortar matrix T lin for wear cond
//(Master side)*/ #define CONTACTFDE_D_MASTER       /* flag for FD check of mortar matrix E lin for
// wear cond (Master side)*/ #define CONTACTFDMORTART_MASTER   /* flag for FD check of mortar matrix
// T (Master side) */ #define CONTACTFDMORTARE_MASTER   /* flag for FD check of mortar matrix E
//(Master side) */

FOUR_C_NAMESPACE_CLOSE

#endif
