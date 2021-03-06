/*$file${.::table.c} #######################################################*/
/*
* Model: dpp_comp.qm
* File:  ${.::table.c}
*
* This code has been generated by QM tool (https://state-machine.com/qm).
* DO NOT EDIT THIS FILE MANUALLY. All your changes will be lost.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*/
/*$endhead${.::table.c} ####################################################*/
#include "qpn.h"  /* QP-nano port */
#include "bsp.h"  /* Board Support Package */
#include "dpp.h"  /* Application interface */

Q_DEFINE_THIS_MODULE("table")

/* Active object class -----------------------------------------------------*/
/*$declare${AOs::Table} ####################################################*/
/*${AOs::Table} ............................................................*/
typedef struct Table {
/* protected: */
    QActive super;

/* private: */
    Philo philo_pool[N_PHILO];
    uint8_t fork[N_PHILO];
    uint8_t isHungry[N_PHILO];
} Table;

/* protected: */
static QState Table_initial(Table * const me);
static QState Table_active(Table * const me);
static QState Table_serving(Table * const me);
static QState Table_paused(Table * const me);
/*$enddecl${AOs::Table} ####################################################*/

/* Global objects ----------------------------------------------------------*/
/*$define${AOs::AO_Table} ##################################################*/
/* Check for the minimum required QP version */
#if ((QP_VERSION < 601) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8)))
#error qpn version 6.0.1 or higher required
#endif
/*${AOs::AO_Table} .........................................................*/
struct Table AO_Table;
/*$enddef${AOs::AO_Table} ##################################################*/

#define RIGHT(n_) ((uint8_t)(((n_) + (N_PHILO - 1U)) % N_PHILO))
#define LEFT(n_)  ((uint8_t)(((n_) + 1U) % N_PHILO))
#define FREE      ((uint8_t)0)
#define USED      ((uint8_t)1)

/*..........................................................................*/
/*$define${AOs::Table_ctor} ################################################*/
/*${AOs::Table_ctor} .......................................................*/
void Table_ctor(void) {
    uint8_t n;
    Table *me = &AO_Table;

    QActive_ctor(&me->super, Q_STATE_CAST(&Table_initial));
    for (n = 0U; n < N_PHILO; ++n) {
        Philo_ctor(&me->philo_pool[n], n);
        me->fork[n] = FREE;
        me->isHungry[n] = 0U;
    }
}
/*$enddef${AOs::Table_ctor} ################################################*/
/*$define${AOs::Table} #####################################################*/
/*${AOs::Table} ............................................................*/
/*${AOs::Table::SM} ........................................................*/
static QState Table_initial(Table * const me) {
    /*${AOs::Table::SM::initial} */
    uint8_t n;
    for (n = 0U; n < N_PHILO; ++n) {
        QHSM_INIT(&me->philo_pool[n].super);
        me->fork[n] = FREE;
        me->isHungry[n] = 0U;
        BSP_displayPhilStat(n, "thinking");
    }
    return Q_TRAN(&Table_serving);
}
/*${AOs::Table::SM::active} ................................................*/
static QState Table_active(Table * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Table::SM::active} */
        case Q_ENTRY_SIG: {
            // request periodic timeout in 1 clock tick and every 1 tick
            QActive_armX(&me->super, 0U, 1U, 1U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::EAT} */
        case EAT_SIG: {
            Q_ERROR();
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::Q_TIMEOUT} */
        case Q_TIMEOUT_SIG: {
            uint8_t n;
            for (n = 0U; n < N_PHILO; ++n) { // for all "Philo" components...
                Philo *philo = &me->philo_pool[n];
                if (philo->tickCtr != 0U) { // is the "Philo" tick counter runnning?
                    if (--philo->tickCtr == 0U) { // is the "Philo" tick expiring?
                        // synchronously dispatch to component
                        Q_SIG(philo) = Q_TIMEOUT_SIG;
                        Q_PAR(philo) = n;
                        QHSM_DISPATCH(&philo->super);
                    }
                }
            }
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/*${AOs::Table::SM::active::serving} .......................................*/
static QState Table_serving(Table * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Table::SM::active::serving} */
        case Q_ENTRY_SIG: {
            uint8_t n;
            for (n = 0U; n < N_PHILO; ++n) { /* give permissions to eat... */
                if ((me->isHungry[n] != 0U)
                    && (me->fork[LEFT(n)] == FREE)
                    && (me->fork[n] == FREE))
                {
                    me->fork[LEFT(n)] = USED;
                    me->fork[n] = USED;

                    /* synchronously dispatch to philo_pool[n] */
                    Q_SIG(&me->philo_pool[n]) = EAT_SIG;
                    Q_PAR(&me->philo_pool[n]) = n;
                    QHSM_DISPATCH(&me->philo_pool[n].super);

                    me->isHungry[n] = 0U;
                    BSP_displayPhilStat(n, "eating  ");
                }
            }
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::serving::HUNGRY} */
        case HUNGRY_SIG: {
            uint8_t n, m;

            n = (uint8_t)Q_PAR(me);
            /* philo ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));

            BSP_displayPhilStat(n, "hungry  ");
            m = LEFT(n);
            /*${AOs::Table::SM::active::serving::HUNGRY::[bothfree]} */
            if ((me->fork[m] == FREE) && (me->fork[n] == FREE)) {
                me->fork[m] = USED;
                me->fork[n] = USED;

                /* synchronously dispatch to philo_pool[n] */
                Q_SIG(&me->philo_pool[n]) = EAT_SIG;
                Q_PAR(&me->philo_pool[n]) = n;
                QHSM_DISPATCH(&me->philo_pool[n].super);

                BSP_displayPhilStat(n, "eating  ");
                status_ = Q_HANDLED();
            }
            /*${AOs::Table::SM::active::serving::HUNGRY::[else]} */
            else {
                me->isHungry[n] = 1U;
                status_ = Q_HANDLED();
            }
            break;
        }
        /*${AOs::Table::SM::active::serving::DONE} */
        case DONE_SIG: {
            uint8_t n, m;

            n = (uint8_t)Q_PAR(me);

            /* phil ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));

            BSP_displayPhilStat(n, "thinking");
            m = LEFT(n);
            /* both forks of Philo[n] must be used */
            Q_ASSERT((me->fork[n] == USED) && (me->fork[m] == USED));

            me->fork[m] = FREE;
            me->fork[n] = FREE;
            m = RIGHT(n); /* check the right neighbor */

            if ((me->isHungry[m] != 0U) && (me->fork[m] == FREE)) {
                me->fork[n] = USED;
                me->fork[m] = USED;
                me->isHungry[m] = 0U;

                /* synchronously dispatch to philo_pool[m] */
                Q_SIG(&me->philo_pool[m]) = EAT_SIG;
                Q_PAR(&me->philo_pool[m]) = m;
                QHSM_DISPATCH(&me->philo_pool[m].super);

                BSP_displayPhilStat(m, "eating  ");
            }
            m = LEFT(n); /* check the left neighbor */
            n = LEFT(m); /* left fork of the left neighbor */
            if ((me->isHungry[m] != 0U) && (me->fork[n] == FREE)) {
                me->fork[m] = USED;
                me->fork[n] = USED;
                me->isHungry[m] = 0U;

                /* synchronously dispatch to philo_pool[m] */
                Q_SIG(&me->philo_pool[m]) = EAT_SIG;
                Q_PAR(&me->philo_pool[m]) = m;
                QHSM_DISPATCH(&me->philo_pool[m].super);

                BSP_displayPhilStat(m, "eating  ");
            }
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::serving::EAT} */
        case EAT_SIG: {
            Q_ERROR();
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::serving::PAUSE} */
        case PAUSE_SIG: {
            status_ = Q_TRAN(&Table_paused);
            break;
        }
        default: {
            status_ = Q_SUPER(&Table_active);
            break;
        }
    }
    return status_;
}
/*${AOs::Table::SM::active::paused} ........................................*/
static QState Table_paused(Table * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Table::SM::active::paused} */
        case Q_ENTRY_SIG: {
            BSP_displayPaused(1U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::paused} */
        case Q_EXIT_SIG: {
            BSP_displayPaused(0U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::paused::SERVE} */
        case SERVE_SIG: {
            status_ = Q_TRAN(&Table_serving);
            break;
        }
        /*${AOs::Table::SM::active::paused::HUNGRY} */
        case HUNGRY_SIG: {
            uint8_t n = (uint8_t)Q_PAR(me);

            /* philo ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));
            me->isHungry[n] = 1U;
            BSP_displayPhilStat(n, "hungry  ");
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::paused::DONE} */
        case DONE_SIG: {
            uint8_t n, m;

            n = (uint8_t)Q_PAR(me);
            /* philo ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));

            BSP_displayPhilStat(n, "thinking");
            m = LEFT(n);
            /* both forks of Phil[n] must be used */
            Q_ASSERT((me->fork[n] == USED) && (me->fork[m] == USED));

            me->fork[m] = FREE;
            me->fork[n] = FREE;
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&Table_active);
            break;
        }
    }
    return status_;
}
/*$enddef${AOs::Table} #####################################################*/
