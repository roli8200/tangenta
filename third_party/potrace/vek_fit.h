/* TANGENTA: schmale Brücke zu Potraces Kurvenstufe, ohne Potraces Makros in C++ */
#ifndef VEK_FIT_H
#define VEK_FIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Ein Segment: corner = 1 -> Gerade zum Eckpunkt c[2..3], dann Gerade nach c[4..5];
   corner = 0 -> Bézierkurve mit Steuerpunkten c[0..3] nach c[4..5]. */
typedef struct vek_fit_segment_s {
    int corner;
    double c[6];
} vek_fit_segment_t;

/* Geschlossener Umriss aus Pixelecken (xy[2*i], xy[2*i+1]), Schritte der Länge 1.
   Liefert die Anzahl Segmente (Speicher mit free freigeben) oder -1 bei einem Fehler. */
int vek_fit(const long* xy, int len, double alphamax, double opttolerance, vek_fit_segment_t** out);

#ifdef __cplusplus
}
#endif

#endif
