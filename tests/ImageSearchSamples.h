#pragma once

// Antworten der Bildarchive (gekürzt) für tst_tools imageSearchLikeThurWiki.
// Eigene Datei: moc liest Raw-Strings in der Testdatei falsch.
namespace sample {
inline const char* const k0 = R"({"query":{"pages":{"9":{"pageid":9,"index":2,"title":"File:B.jpg","imageinfo":[{"url":"https://u/B.jpg","thumburl":"https://u/1600px-B.jpg","width":4000,"height":3000,"descriptionurl":"https://c/B","extmetadata":{"Artist":{"value":"<a href=\"x\">Anna</a>"},"LicenseShortName":{"value":"CC BY 4.0"}}}]},"7":{"pageid":7,"index":1,"title":"File:A.jpg","imageinfo":[{"url":"https://u/A.jpg","width":10,"height":10,"extmetadata":{}}]}}}})";
inline const char* const k1 = R"({"query":{"pages":{"9":{"pageid":9,"imageinfo":[{"thumburl":"https://u/330px-B.jpg"}]}}}})";
inline const char* const k2 = R"({"results":[{"id":"x1","title":"Rack","url":"https://f/r.jpg","thumbnail":"https://o/t","creator":"Bob","license":"by-sa","license_version":"2.0","foreign_landing_url":"https://f/p","width":800,"height":600}]})";
inline const char* const k3 = R"({"success":true,"icons":[{"id":"39210","name":"Monitor","commonName":"monitor","platform":"ios7"},{"id":"anim","name":"Monitor","commonName":"monitor--v2","isAnimated":true}]})";
inline const char* const k4 = R"({"data":[{"id":30955,"name":"monitor","slug":"monitor","free_svg":true,"thumbnails":[{"url":"https://cdn/128.png","width":128,"height":128}],"style":{"name":"Lineal"},"author":{"name":"Freepik"}},{"id":2,"name":"monitor","free_svg":false,"thumbnails":[{"url":"https://cdn/p.png","width":128,"height":128}],"author":{"name":"X"}}]})";
inline const char* const k5 = R"({"data":[{"id":5,"title":"Desk","url":"https://m/5","image":{"source":{"url":"https://m/5.jpg","size":"626x417"}},"licenses":[{"type":"freemium"}],"author":{"name":"M"}},{"id":6,"title":"Premium","image":{"source":{"url":"https://m/6.jpg"}},"licenses":[{"type":"premium"}]}]})";
inline const char* const k6 = R"({"hits":[{"id":1,"tags":"a, b","largeImageURL":"https://p/l.jpg","previewURL":"https://p/s.jpg","user":"U"}]})";
inline const char* const k7 = R"({"results":[{"id":"u1","alt_description":"desk","width":5000,"height":3000,"urls":{"raw":"https://s/raw?ixid=1","full":"https://s/f","regular":"https://s/r","thumb":"https://s/t"},"user":{"name":"N"}}]})";
inline const char* const k8 = R"({"photos":[{"id":3,"alt":"rack","width":4000,"height":6000,"src":{"original":"https://x/o.jpeg","large":"https://x/l","tiny":"https://x/t"},"photographer":"P"}]})";
inline const char* const k9 = R"({"images":[{"id":"g1","title":"T","display_sizes":[{"uri":"https://g/s"},{"uri":"https://g/l"}],"artist":"A"}]})";
inline const char* const k10 = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><path d="M2 4h20v12H2z M4 6v8h16V6z" fill="#000"/></svg>)";
} // namespace sample
