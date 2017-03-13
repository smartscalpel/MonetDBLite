/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2008-2017 MonetDB B.V.
 */

#include "jresulset.h"
#include "monetdb_config.h"
#include "res_table.h"
#include "mal_type.h"
#include "gdk.h"
#include "embedded.h"

JResultSet* createResultSet(res_table* output) {
    JResultSet* thisResultSet = (JResultSet*) GDKmalloc(sizeof(JResultSet));
    int numberOfColumns = output->nr_cols, *quickerDigits, *quickerScales, i;
    BAT** dearBats;
    res_col col;
    if(thisResultSet != NULL) {
        thisResultSet->output = output;
        thisResultSet->bats = (BAT**) GDKmalloc(sizeof(BAT*) * numberOfColumns);
        thisResultSet->digits = (int*) GDKmalloc(sizeof(int) * numberOfColumns);
        thisResultSet->scales = (int*) GDKmalloc(sizeof(int) * numberOfColumns);

        if(thisResultSet->bats == NULL || thisResultSet->digits == NULL || thisResultSet->scales == NULL) {
            if(thisResultSet->bats != NULL) {
                GDKfree(thisResultSet->bats);
            }
            if(thisResultSet->digits != NULL) {
                GDKfree(thisResultSet->digits);
            }
            if(thisResultSet->scales != NULL) {
                GDKfree(thisResultSet->scales);
            }
            GDKfree(thisResultSet);
            thisResultSet = NULL;
        } else {
            dearBats = thisResultSet->bats;
            quickerDigits = thisResultSet->digits;
            quickerScales = thisResultSet->scales;
            for (i = 0; i < numberOfColumns; i++) {
                col = output->cols[i];
                dearBats[i] = BATdescriptor(col.b);
                quickerDigits[i] = (int) col.type.digits;
                quickerScales[i] = (int) col.type.scale;
            }
        }
    }
    return thisResultSet;
}

void freeResultSet(JResultSet* thisResultSet) {
    if(thisResultSet != NULL) {
        int numberOfColumns = thisResultSet->output->nr_cols, i;
        BAT **dearBats = thisResultSet->bats;

        for (i = 0; i < numberOfColumns; i++) {
            BBPunfix(dearBats[i]->batCacheid);
        }
        GDKfree(dearBats);
        GDKfree(thisResultSet->digits);
        GDKfree(thisResultSet->scales);
        monetdb_cleanup_result(NULL, thisResultSet->output);
        GDKfree(thisResultSet);
    }
}
