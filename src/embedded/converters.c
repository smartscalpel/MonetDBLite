/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2008-2017 MonetDB B.V.
 */

#include "converters.h"

#include "gdk_utils.h"
#include "sql.h"
#include <jni.h>
#include <string.h>
#include <stdlib.h>

#define DO_NOTHING                        ;
#define BIG_INTEGERS_ARRAY_SIZE           64
#define min(a, b)                         (((a) < (b)) ? (a) : (b))

static jbyte jbyteMin;
static jshort jshortMin;
static jint jintMin;
static jlong jlongMin;
static jfloat jfloatMin;
static jdouble jdoubleMin;
static str str_nil_cast = (str) str_nil;

#define VOID_CONSTRUCTORS                 (*env)->NewObject(env, jClass, constructor)
#define NATIVE_TYPE_CONSTRUCTORS          (*env)->NewObject(env, jClass, constructor, value)

//this function is used to load the JVM minimum values of primitives so we will use them to map MonetDB's NULL values
void initializeMinimums(jbyte m1, jshort m2, jint m3, jlong m4, jfloat m5, jdouble m6) {
    jbyteMin = m1;
    jshortMin = m2;
    jintMin = m3;
    jlongMin = m4;
    jfloatMin = m5;
    jdoubleMin = m6;
}

/* ------------------------------------------------------------------------------- Converting BATs to Java Classes and primitives ------------------------------------------------------------------------------ */

/* For primitive types the mapping is direct <3 */

#define DIRECT_MAPPING(NAME, JAVA_CAST, CAPITAL_NAME, NULL_CONST, JAVA_MIN) \
    void get##NAME##Column(JNIEnv *env, JAVA_CAST##Array result, int first, int last, BAT* b) { \
        int size = last - first; \
        const JAVA_CAST* array = (const JAVA_CAST*) Tloc(b, 0); \
        array += first; \
        if (b->tnonil && !b->tnil) { \
            (*env)->Set##CAPITAL_NAME##ArrayRegion(env, result, first, size, array); \
        } else { \
            JAVA_CAST minimum = JAVA_MIN; \
            JAVA_CAST* aux = (*env)->Get##CAPITAL_NAME##ArrayElements(env, result, NULL); \
            for(int i = 0 ; i < last ; i++) { \
                if (array[i] == NULL_CONST) { \
                    aux[i] = minimum; \
                } else { \
                    aux[i] = array[i]; \
                } \
            } \
            (*env)->Release##CAPITAL_NAME##ArrayElements(env, result, aux, 0); \
        } \
    }

DIRECT_MAPPING(Boolean, jbyte, Byte, bte_nil, jbyteMin)
DIRECT_MAPPING(Tinyint, jbyte, Byte, bte_nil, jbyteMin)
DIRECT_MAPPING(Smallint, jshort, Short, sht_nil, jshortMin)
DIRECT_MAPPING(Int, jint, Int, int_nil, jintMin)
DIRECT_MAPPING(Bigint, jlong, Long, lng_nil, jlongMin)
DIRECT_MAPPING(Real, jfloat, Float, flt_nil, jfloatMin)
DIRECT_MAPPING(Double, jdouble, Double, dbl_nil, jdoubleMin)

/* For these we have to create Java objects :( */

#define GET_NEXT_JDATE          long value = ((long) nvalue - 719528L) * 86400000L; //number of days since jan first (year 0) to number of milliseconds since 1 jan 1970
#define CHECK_NULL_BDATE        nvalue != date_nil

#define GET_NEXT_JTIME          long value = (long) nvalue - 3600000L; //convert number of milliseconds since start of the day to to number of milliseconds since 1 jan 1970
#define CHECK_NULL_BTIME        nvalue != daytime_nil

#define GET_NEXT_JTIMESTAMP     long value = ((long) nvalue.payload.p_days - 719528L) * 86400000L + (long) nvalue.payload.p_msecs - 3600000L;
#define CHECK_NULL_BTIMESTAMP   !ts_isnil(nvalue)

#define GREGORIAN_EXTRA         (*env)->CallVoidMethod(env, next, setter, value);

#define CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(NAME, BAT_CAST, CONVERT, NULL_CMP, CONSTRUCTOR, EXTRA_STEP) \
    void get##NAME##Column(JNIEnv *env, jobjectArray result, jclass jClass, jmethodID constructor, jmethodID setter, int first, int last, BAT* b) { \
        BAT_CAST *array = (BAT_CAST *) Tloc(b, 0); \
        if (b->tnonil && !b->tnil) { \
            for (int i = first, j = 0; i < last; i++, j++) { \
                BAT_CAST nvalue = array[i]; \
                CONVERT \
                jobject next = CONSTRUCTOR; \
                EXTRA_STEP \
                (*env)->SetObjectArrayElement(env, result, j, next); \
                (*env)->DeleteLocalRef(env, next); \
            } \
        } else { \
            for (int i = first, j = 0; i < last; i++, j++) { \
                BAT_CAST nvalue = array[i]; \
                if(NULL_CMP) { \
                    CONVERT \
                    jobject next = CONSTRUCTOR; \
                    EXTRA_STEP \
                    (*env)->SetObjectArrayElement(env, result, j, next); \
                    (*env)->DeleteLocalRef(env, next); \
                } \
            } \
        } \
    }

CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(Date, int, GET_NEXT_JDATE, CHECK_NULL_BDATE, NATIVE_TYPE_CONSTRUCTORS, DO_NOTHING)
CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(Time, int, GET_NEXT_JTIME, CHECK_NULL_BTIME, NATIVE_TYPE_CONSTRUCTORS, DO_NOTHING)
CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(Timestamp, timestamp, GET_NEXT_JTIMESTAMP, CHECK_NULL_BTIMESTAMP, NATIVE_TYPE_CONSTRUCTORS, DO_NOTHING)
CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(GregorianCalendarDate, int, GET_NEXT_JDATE, CHECK_NULL_BDATE, VOID_CONSTRUCTORS, GREGORIAN_EXTRA)
CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(GregorianCalendarTime, int, GET_NEXT_JTIME, CHECK_NULL_BTIME, VOID_CONSTRUCTORS, GREGORIAN_EXTRA)
CONVERT_EASIER_BAT_TO_JCLASS_ARRAY(GregorianCalendarTimestamp, timestamp, GET_NEXT_JTIMESTAMP, CHECK_NULL_BTIMESTAMP, VOID_CONSTRUCTORS, GREGORIAN_EXTRA)

/* For decimals the conversion is harder!! */

static void
decimal_to_str_java(char* value, lng v, int scale) 
{
	char buf[BIG_INTEGERS_ARRAY_SIZE];
	int cur = 63, neg = (v<0), i, done = 0;

	if (v<0) v = -v;

	buf[cur--] = 0;
	if (scale){
		for (i=0; i<scale; i++) {
			buf[cur--] = (char) (v%10 + '0');
			v /= 10;
		}
		buf[cur--] = '.';
	}
	while (v) {
		buf[cur--] = (char ) (v%10 + '0');
		v /= 10;
		done = 1;
	}
	if (!done)
		buf[cur--] = '0';
	if (neg)
		buf[cur--] = '-';
	assert(cur >= -1);
	strcpy(value, buf+cur+1);
}

#define CONVERT_BAT_TO_JDECIMAL_ARRAY(BAT_CAST, CONVERSION_CAST) \
    void getDecimal##BAT_CAST##Column(JNIEnv *env, jobjectArray result, jclass jClass, jmethodID constructor, int first, int last, BAT* b, int scale) { \
        char value[BIG_INTEGERS_ARRAY_SIZE]; \
        const BAT_CAST *array = (const BAT_CAST *) Tloc(b, 0); \
        if (b->tnonil && !b->tnil) { \
            for (int i = first, j = 0; i < last; i++, j++) { \
                decimal_to_str_java(value, (CONVERSION_CAST) array[i], scale); \
                jstring aux = (*env)->NewStringUTF(env, value); \
                jobject next = (*env)->NewObject(env, jClass, constructor, aux); \
                (*env)->SetObjectArrayElement(env, result, j, next); \
                (*env)->DeleteLocalRef(env, aux); \
                (*env)->DeleteLocalRef(env, next); \
            } \
        } else { \
            for (int i = first, j = 0; i < last; i++, j++) { \
                BAT_CAST nvalue = array[i]; \
                if(nvalue != BAT_CAST##_nil) { \
                    decimal_to_str_java(value, nvalue, scale); \
                    jstring aux = (*env)->NewStringUTF(env, value); \
                    jobject next = (*env)->NewObject(env, jClass, constructor, aux); \
                    (*env)->SetObjectArrayElement(env, result, j, next); \
                    (*env)->DeleteLocalRef(env, aux); \
                    (*env)->DeleteLocalRef(env, next); \
                } \
            } \
        } \
    }

CONVERT_BAT_TO_JDECIMAL_ARRAY(bte, lng)
CONVERT_BAT_TO_JDECIMAL_ARRAY(sht, lng)
CONVERT_BAT_TO_JDECIMAL_ARRAY(int, lng)
CONVERT_BAT_TO_JDECIMAL_ARRAY(lng, lng)

/* For the next ones, we must retrieve from the BAT's heap */

#define BAT_TO_JSTRING_F         str nvalue = BUNtail(li, p); /* For Char, Varchar, Clob and URLs */
#define BAT_TO_JSTRING_S         jstring value = (*env)->NewStringUTF(env, nvalue);
#define CHECK_NULL_BSTRING       strcmp(str_nil_cast, nvalue) != 0

#define BAT_TO_JBLOB_F           blob* nvalue = (blob*) BUNtail(li, p);
#define BAT_TO_JBLOB_S           jbyteArray value = (*env)->NewByteArray(env, nvalue->nitems); \
                                 (*env)->SetByteArrayRegion(env, value, 0, nvalue->nitems, (jbyte*)nvalue->data); /* For BLOBs */
#define CHECK_NULL_BBLOB         nvalue->nitems != ~(size_t) 0

#define CONVERT_HARDER_BAT_TO_JCLASS_ARRAY(NAME, RETRIEVE_BAT, CONVERT_BAT, NULL_CMP) \
    void get##NAME##Column(JNIEnv *env, jobjectArray result, jclass jClass, jmethodID constructor, int first, int last, BAT* b) { \
       int j = 0; \
       BATiter li = bat_iterator(b); \
       if (b->tnonil && !b->tnil) { \
           for (BUN p = (BUN) first, q = (BUN) last; p < q; p++) { \
               RETRIEVE_BAT \
               CONVERT_BAT \
               (*env)->SetObjectArrayElement(env, result, j, value); \
               j++; \
               (*env)->DeleteLocalRef(env, value); \
           } \
       } else { \
           for (BUN p = (BUN) first, q = (BUN) last; p < q; p++) { \
               RETRIEVE_BAT \
               if (NULL_CMP) { \
                   CONVERT_BAT \
                   (*env)->SetObjectArrayElement(env, result, j, value); \
                   (*env)->DeleteLocalRef(env, value); \
               } \
               j++; \
           } \
       } \
   }

CONVERT_HARDER_BAT_TO_JCLASS_ARRAY(String, BAT_TO_JSTRING_F, BAT_TO_JSTRING_S, CHECK_NULL_BSTRING)
CONVERT_HARDER_BAT_TO_JCLASS_ARRAY(Blob, BAT_TO_JBLOB_F, BAT_TO_JBLOB_S, CHECK_NULL_BBLOB)

/* ------------------------------------------------------------------------------------  Converting Java Classes and primitives to BATs  ------------------------------------------------------------------ */

/* Direct mapping for primitives :) */

#define DIRECT_CONVERSION(NAME, BAT_CAST, JAVA_CAST, COPY_METHOD) \
    void store##NAME##Column(JNIEnv *env, BAT** b, JAVA_CAST##Array data, size_t cnt, int localtype) { \
        BAT *aux = COLnew(0, localtype, cnt, TRANSIENT); \
        if (!aux) { \
            *b = NULL; \
            return; \
        } \
        aux->tnil = 0; \
        aux->tnonil = 1; \
        aux->tkey = 0; \
        aux->tsorted = 1; \
        aux->trevsorted = 1; \
        aux->tdense = 0; \
        BAT_CAST *p = (BAT_CAST *) Tloc(aux, 0); \
        (*env)->Get##COPY_METHOD##ArrayRegion(env, data, 0, cnt, (JAVA_CAST *) p); \
        BAT_CAST value, prev = BAT_CAST##_nil; \
        for(size_t i = 0; i < cnt; i++, p++) { \
            value = p[i]; \
            if (value == BAT_CAST##_nil) { \
                aux->tnil = 1; \
                aux->tnonil = 0; \
            } \
            if (i > 0) { \
                if (value > prev && aux->trevsorted) { \
                    aux->trevsorted = 0; \
                } else if (value < prev && aux->tsorted) { \
                    aux->tsorted = 0; \
                } \
            } \
            prev = value; \
        } \
        BATsetcount(aux, cnt); \
        BATsettrivprop(aux); \
        BBPkeepref(aux->batCacheid); \
        *b = aux; \
    }

DIRECT_CONVERSION(Boolean, bit, jbyte, Byte)
DIRECT_CONVERSION(Tinyint, bte, jbyte, Byte)
DIRECT_CONVERSION(Smallint, sht, jshort, Short)
DIRECT_CONVERSION(Int, int, jint, Int)
DIRECT_CONVERSION(Bigint, lng, jlong, Long)
DIRECT_CONVERSION(Real, flt, jfloat, Float)
DIRECT_CONVERSION(Double, dbl, jdouble, Double)

/* These types have constant sizes, so the conversion is still easy :P */

#define EASY_CMP                 *p - prev

#define JDATE_TO_BAT             long nvalue = (*env)->CallLongMethod(env, value, conversor); \
                                 if(nvalue > 0) { \
                                     nvalue += 86400000L; \
                                 } \
                                 *p = (date) (nvalue / 86400000L + 719528L);

#define JTIME_TO_BAT             long nvalue = (*env)->CallLongMethod(env, value, conversor); \
                                 *p = (daytime) (nvalue + 3600000L);

#define JTIMESTAMP_TO_BAT        long nvalue = (*env)->CallLongMethod(env, value, conversor); \
                                 ldiv_t aux1 = ldiv(nvalue, 86400000L);                       \
                                 p->payload.p_days = (date) (aux1.quot + 719528L);            \
                                 p->payload.p_msecs = (daytime) (aux1.rem + 3600000L);
#define TIMESTAMP_CMP            p->payload.p_days + p->payload.p_msecs - prev.payload.p_days - prev.payload.p_msecs

#define CONVERT_EASY_JCLASS_ARRAY_TO_BAT(NAME, BAT_CAST, NULL_CONST, CONVERT_TO_BAT, ORDER_CMP) \
    void store##NAME##Column(JNIEnv *env, BAT** b, jobjectArray data, jmethodID conversor, size_t cnt, int localtype) { \
        BAT *aux = COLnew(0, localtype, cnt, TRANSIENT); \
        if (!aux) { \
            *b = NULL; \
            return; \
        } \
        aux->tnil = 0; \
        aux->tnonil = 1; \
        aux->tkey = 0; \
        aux->tsorted = 1; \
        aux->trevsorted = 1; \
        aux->tdense = 0; \
        BAT_CAST *p = (BAT_CAST *) Tloc(aux, 0); \
        BAT_CAST prev = NULL_CONST; \
        jobject value; \
        for(size_t i = 0; i < cnt; i++, p++) { \
            value = (*env)->GetObjectArrayElement(env, data, i); \
            if (value == NULL) { \
                aux->tnil = 1; \
                aux->tnonil = 0; \
                *p = NULL_CONST; \
            } else { \
                CONVERT_TO_BAT \
                (*env)->DeleteLocalRef(env, value); \
            } \
            if (i > 0) { \
                if ((ORDER_CMP > 0) && aux->trevsorted) { \
                    aux->trevsorted = 0; \
                } else if ((ORDER_CMP < 0) && aux->tsorted) { \
                    aux->tsorted = 0; \
                } \
            } \
            prev = *p; \
        } \
        BATsetcount(aux, cnt); \
        BATsettrivprop(aux); \
        BBPkeepref(aux->batCacheid); \
        *b = aux; \
    }

CONVERT_EASY_JCLASS_ARRAY_TO_BAT(Date, date, date_nil, JDATE_TO_BAT, EASY_CMP)
CONVERT_EASY_JCLASS_ARRAY_TO_BAT(Time, daytime, daytime_nil, JTIME_TO_BAT, EASY_CMP)
CONVERT_EASY_JCLASS_ARRAY_TO_BAT(Timestamp, timestamp, *timestamp_nil, JTIMESTAMP_TO_BAT, TIMESTAMP_CMP)

/* Decimals are harder :P */

static lng
decimal_from_str_java(const char *dec)
{
    lng res = 0;
    const lng max0 = GDK_lng_max / 10, max1 = GDK_lng_max % 10;
    int neg = 0;

    while(isspace(*dec))
        dec++;
    if (*dec == '-') {
        neg = 1;
        dec++;
    } else if (*dec == '+') {
        dec++;
    }
    for (; *dec && ((*dec >= '0' && *dec <= '9') || *dec == '.'); dec++) {
        if (*dec != '.') {
            if (res > max0 || (res == max0 && *dec - '0' > max1))
                break;
            res *= 10;
            res += *dec - '0';
        }
    }
    while(isspace(*dec))
        dec++;
    if (neg)
        return -res;
    else
        return res;
}

#define CONVERT_JDECIMAL_ARRAY_TO_BAT(BAT_CAST) \
    void storeDecimal##BAT_CAST##Column(JNIEnv *env, BAT** b, jobjectArray data, jmethodID conversor, jmethodID setScale, size_t cnt, int localtype, int scale, int roundingMode) { \
        BAT *aux = COLnew(0, localtype, cnt, TRANSIENT); \
        if (!aux) { \
            *b = NULL; \
            return; \
        } \
        aux->tnil = 0; \
        aux->tnonil = 1; \
        aux->tkey = 0; \
        aux->tsorted = 1; \
        aux->trevsorted = 1; \
        aux->tdense = 0; \
        BAT_CAST *p = (BAT_CAST *) Tloc(aux, 0); \
        BAT_CAST prev = BAT_CAST##_nil; \
        for(size_t i = 0; i < cnt; i++, p++) { \
            jobject value = (*env)->GetObjectArrayElement(env, data, i); \
            if (value == NULL) { \
                aux->tnil = 1; \
                aux->tnonil = 0; \
                *p = BAT_CAST##_nil; \
            } else { \
                jobject aux = (*env)->CallObjectMethod(env, value, setScale, scale, roundingMode); \
                jstring nvalue = (*env)->CallObjectMethod(env, aux, conversor); \
                const char *representation = (*env)->GetStringUTFChars(env, nvalue, NULL); \
                *p = (BAT_CAST) decimal_from_str_java(representation); \
                (*env)->ReleaseStringUTFChars(env, nvalue, representation); \
                (*env)->DeleteLocalRef(env, nvalue); \
                (*env)->DeleteLocalRef(env, aux); \
                (*env)->DeleteLocalRef(env, value); \
            } \
            if (i > 0) { \
                if (*p - prev > 0 && aux->trevsorted) { \
                    aux->trevsorted = 0; \
                } else if (*p - prev < 0 && aux->tsorted) { \
                    aux->tsorted = 0; \
                } \
            } \
            prev = *p; \
        } \
        BATsetcount(aux, cnt); \
        BATsettrivprop(aux); \
        BBPkeepref(aux->batCacheid); \
        *b = aux; \
    }

CONVERT_JDECIMAL_ARRAY_TO_BAT(bte)
CONVERT_JDECIMAL_ARRAY_TO_BAT(sht)
CONVERT_JDECIMAL_ARRAY_TO_BAT(int)
CONVERT_JDECIMAL_ARRAY_TO_BAT(lng)

static blob *
my_blob_null(void) {
    static blob mynullval;
    mynullval.nitems = ~(size_t) 0;
    return (&mynullval);
}

/* Put in the BAT's heap :S */

#define JSTRING_TO_BAT      int len = (*env)->GetStringUTFLength(env, value);  \
                            p = GDKmalloc(len);                                \
                            (*env)->GetStringUTFRegion(env, value, 0, len, p);

#define PUT_STR_IN_HEAP     BUNappend(aux, p, FALSE);

#define STR_CMP             strcmp(p, prev)

#define JBLOB_TO_BAT        jbyteArray nvalue = (jbyteArray) value;                   \
                            jsize len = (*env)->GetArrayLength(env, nvalue);          \
                            p = GDKmalloc(blobsize(len));                             \
                            p->nitems = len;                                          \
                            (*env)->GetByteArrayRegion(env, nvalue, 0, len, (jbyte*)p->data);

#define BLOB_START          var_t bun_offset = 0;

#define PUT_BLOB_IN_HEAP    BLOBput(aux->tvheap, &bun_offset, p); \
                            BUNappend(aux, p, FALSE);

#define BLOB_CMP            memcmp(p->data, prev->data, min(p->nitems, prev->nitems))

#define CONVERT_HARDER_JCLASS_ARRAY_TO_BAT(NAME, BAT_CAST, NULL_CONST, START_STEP, CONVERT_TO_BAT, ORDER_CMP, PUT_IN_HEAP) \
    void store##NAME##Column(JNIEnv *env, BAT** b, jobjectArray data, jmethodID conversor, size_t cnt, int localtype) { \
        BAT *aux = COLnew(0, localtype, cnt, TRANSIENT); \
        if (!aux) { \
            *b = NULL; \
            return; \
        } \
        aux->tnil = 0; \
        aux->tnonil = 1; \
        aux->tkey = 0; \
        aux->tsorted = 1; \
        aux->trevsorted = 1; \
        BAT_CAST p; \
        BAT_CAST prev = NULL_CONST; \
        START_STEP \
        int previousToFree = 0; \
        for(size_t i = 0; i < cnt; i++) { \
            jobject value = (*env)->GetObjectArrayElement(env, data, i); \
            if (value == NULL) { \
                aux->tnil = 1; \
                aux->tnonil = 0; \
                p = NULL_CONST; \
            } else { \
                CONVERT_TO_BAT \
                (*env)->DeleteLocalRef(env, value); \
            } \
            PUT_IN_HEAP \
            if (i > 0) { \
                if ((ORDER_CMP > 0) && aux->trevsorted) { \
                    aux->trevsorted = 0; \
                } else if ((ORDER_CMP < 0) && aux->tsorted) { \
                    aux->tsorted = 0; \
                } \
                if(previousToFree) { \
                    GDKfree(prev); \
                } \
            } \
            prev = p; \
            previousToFree = (value == NULL) ? 0 : 1; \
        } \
        if(previousToFree) { \
            GDKfree(p); \
        } \
        BATsetcount(aux, cnt); \
        BATsettrivprop(aux); \
        BBPkeepref(aux->batCacheid); \
        *b = aux; \
    }

CONVERT_HARDER_JCLASS_ARRAY_TO_BAT(String, str, str_nil_cast, DO_NOTHING, JSTRING_TO_BAT, STR_CMP, PUT_STR_IN_HEAP)
CONVERT_HARDER_JCLASS_ARRAY_TO_BAT(Blob, blob*, my_blob_null(), BLOB_START, JBLOB_TO_BAT, BLOB_CMP, PUT_BLOB_IN_HEAP)
