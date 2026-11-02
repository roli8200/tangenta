/* TANGENTA: schmale Brücke zu Potraces Kurvenstufe (process_path) */
#include <stdlib.h>
#include <string.h>

#include "curve.h"
#include "progress.h"
#include "trace.h"
#include "vek_fit.h"

int vek_fit(const long* xy, int len, double alphamax, double opttolerance, vek_fit_segment_t** out)
{
    path_t* p;
    potrace_param_t param;
    progress_t progress;
    int i, n;

    *out = NULL;
    if (len < 4)
        return -1;
    p = path_new();
    if (!p)
        return -1;
    p->sign = '+';
    p->area = 0;
    p->priv->len = len;
    p->priv->pt = (point_t*)malloc(sizeof(point_t) * (size_t)len);
    if (!p->priv->pt) {
        path_free(p);
        return -1;
    }
    for (i = 0; i < len; ++i) {
        p->priv->pt[i].x = xy[2 * i];
        p->priv->pt[i].y = xy[2 * i + 1];
    }
    memset(&param, 0, sizeof(param));
    param.turdsize = 0;
    param.turnpolicy = POTRACE_TURNPOLICY_MINORITY;
    param.alphamax = alphamax;
    param.opticurve = 1;
    param.opttolerance = opttolerance;
    memset(&progress, 0, sizeof(progress));
    if (process_path(p, &param, &progress)) {
        path_free(p);
        return -1;
    }
    n = p->curve.n;
    *out = (vek_fit_segment_t*)malloc(sizeof(vek_fit_segment_t) * (size_t)(n > 0 ? n : 1));
    if (!*out) {
        path_free(p);
        return -1;
    }
    for (i = 0; i < n; ++i) {
        int k;
        (*out)[i].corner = p->curve.tag[i] == POTRACE_CORNER;
        for (k = 0; k < 3; ++k) {
            (*out)[i].c[2 * k] = p->curve.c[i][k].x;
            (*out)[i].c[2 * k + 1] = p->curve.c[i][k].y;
        }
    }
    path_free(p);
    return n;
}
