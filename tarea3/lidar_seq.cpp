// Practica 2 - Ingenieria de los Computadores
// (Copiado a Practica 3 como version SECUENCIAL de referencia; solo se ha
//  anadido la linea CSV final para recoger resultados.)
// Deteccion de obstaculos con LIDAR para frenado de emergencia (coche autonomo)
//
// Simula lo que devolveria un sensor LIDAR tras segmentar un obstaculo en la
// carretera (un grupo de peatones, un coche o escombros): una nube de N
// puntos 3D agrupados alrededor de un centro, a una distancia dada del coche.
//
// Sobre esa nube se calculan tres analisis independientes (leen la misma
// nube, no la modifican, y no dependen entre si):
//
//   Tarea A: centroide y caja delimitadora -> tamano del objeto y hacia donde
//            se mueve.
//   Tarea B: ordenacion por profundidad -> el primer punto tras ordenar es el
//            mas cercano al parachoques (dispara el frenado de emergencia).
//   Tarea C: par de puntos mas cercano entre si -> mide la "densidad" del
//            objeto (solido = frenar ya; disperso = posible humo o lluvia).
//
// Uso: ./lidar_brake <n_puntos> <dispersion> <distancia_m> [semilla]

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <limits>

struct Point3D {
    double x, y, z; // x: lateral, y: altura, z: profundidad (distancia al coche)
};

struct BoundingBox {
    double min_x, max_x, min_y, max_y, min_z, max_z;
};

// Genera la nube de puntos que representaria el obstaculo segmentado por el
// LIDAR. La dispersion controla tanto el tamano del objeto como su densidad:
// valores pequenos -> objeto compacto (camion solido); valores grandes ->
// objeto disperso (humo, lluvia intensa, arbusto).
static std::vector<Point3D> generar_obstaculo(int n, double dispersion, double distancia, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> lateral(0.0, dispersion);
    std::normal_distribution<double> altura(1.0, dispersion * 0.5);
    std::normal_distribution<double> profundidad(distancia, dispersion * 0.3);

    std::vector<Point3D> nube;
    nube.reserve(n);
    for (int i = 0; i < n; ++i) {
        nube.push_back({lateral(rng), altura(rng), profundidad(rng)});
    }
    return nube;
}

// Tarea A: centroide + bounding box. O(n), paralelismo de datos (reduccion:
// suma y min/max se pueden acumular en paralelo y combinar al final).
static void centroide_y_bbox(const std::vector<Point3D>& nube, Point3D& centroide, BoundingBox& caja) {
    double sx = 0, sy = 0, sz = 0;
    caja.min_x = caja.min_y = caja.min_z = std::numeric_limits<double>::max();
    caja.max_x = caja.max_y = caja.max_z = std::numeric_limits<double>::lowest();

    for (const auto& p : nube) {
        sx += p.x; sy += p.y; sz += p.z;
        caja.min_x = std::min(caja.min_x, p.x); caja.max_x = std::max(caja.max_x, p.x);
        caja.min_y = std::min(caja.min_y, p.y); caja.max_y = std::max(caja.max_y, p.y);
        caja.min_z = std::min(caja.min_z, p.z); caja.max_z = std::max(caja.max_z, p.z);
    }
    size_t n = nube.size();
    centroide = {sx / n, sy / n, sz / n};
}

// Tarea B: ordenacion por profundidad. O(n log n). Tras ordenar, nube.front()
// es el punto mas cercano al parachoques.
static void ordenar_por_profundidad(std::vector<Point3D>& nube) {
    std::sort(nube.begin(), nube.end(),
              [](const Point3D& a, const Point3D& b) { return a.z < b.z; });
}

// Tarea C: par de puntos mas cercano por fuerza bruta. O(n^2): cada par es
// independiente de los demas (paralelismo de datos). Es la tarea que domina
// el tiempo total de ejecucion.
static double distancia_minima_entre_puntos(const std::vector<Point3D>& nube) {
    double mejor = std::numeric_limits<double>::max();
    size_t n = nube.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            double dx = nube[i].x - nube[j].x;
            double dy = nube[i].y - nube[j].y;
            double dz = nube[i].z - nube[j].z;
            double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < mejor) mejor = d2;
        }
    }
    return std::sqrt(mejor);
}

static double ahora_seg() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Uso: %s <n_puntos> <dispersion> <distancia_m> [semilla]\n", argv[0]);
        return 1;
    }
    int n = std::atoi(argv[1]);
    double dispersion = std::atof(argv[2]);
    double distancia = std::atof(argv[3]);
    unsigned seed = (argc > 4) ? static_cast<unsigned>(std::atoi(argv[4])) : 42u;

    // Umbral de densidad para clasificar solido vs disperso (metros).
    // Valor elegido para esta escala de dispersion; ajustadlo si cambiais
    // los parametros por defecto del caso de prueba.
    const double UMBRAL_SOLIDO = 0.05;

    double t0 = ahora_seg();
    std::vector<Point3D> nube = generar_obstaculo(n, dispersion, distancia, seed);
    double t1 = ahora_seg();

    Point3D centroide; BoundingBox caja;
    centroide_y_bbox(nube, centroide, caja);
    double t2 = ahora_seg();

    ordenar_por_profundidad(nube);
    double t3 = ahora_seg();

    double d_min = distancia_minima_entre_puntos(nube);
    double t4 = ahora_seg();

    std::printf("=== Deteccion de obstaculo LIDAR ===\n");
    std::printf("Puntos analizados: %d\n", n);
    std::printf("Centroide: (%.2f, %.2f, %.2f) m\n", centroide.x, centroide.y, centroide.z);
    std::printf("Bounding box: ancho=%.2f m, alto=%.2f m, profundidad=%.2f m\n",
                 caja.max_x - caja.min_x, caja.max_y - caja.min_y, caja.max_z - caja.min_z);
    std::printf("Punto mas cercano al parachoques: %.2f m\n", nube.front().z);
    std::printf("Distancia minima entre puntos: %.4f m -> objeto %s\n",
                 d_min, (d_min < UMBRAL_SOLIDO ? "SOLIDO (frenar)" : "DIFUSO (posible ruido/lluvia)"));
    std::printf("\n--- Tiempos ---\n");
    std::printf("Generacion nube:        %.4f s\n", t1 - t0);
    std::printf("Centroide + bbox:       %.4f s\n", t2 - t1);
    std::printf("Orden por profundidad:  %.4f s\n", t3 - t2);
    std::printf("Par mas cercano:        %.4f s\n", t4 - t3);
    std::printf("TOTAL (sin generacion): %.4f s\n", t4 - t1);
    // estrategia,P,n,total,bcast,comp_max,comp_min,reduce,dmin
    std::printf("CSV,secuencial,1,%d,%.6f,0,%.6f,%.6f,0,%.6f\n", n, t4 - t1, t4 - t3, t4 - t3, d_min);

    return 0;
}
