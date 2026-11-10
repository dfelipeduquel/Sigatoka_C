/**
 * @file EmaFilter.h
 * @brief Filtro de Media Móvil Exponencial (EMA) para acondicionamiento de señales analógicas.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_EMA_FILTER_H
#define SIGATOK_EMA_FILTER_H

#include <Arduino.h>

class EmaFilter {
public:
    /**
     * @brief Constructor del filtro EMA.
     * @param alpha Factor de ponderación (0.0 < alpha <= 1.0). Menor valor = mayor filtrado de ruido.
     */
    explicit EmaFilter(float alpha = 0.15f);

    /**
     * @brief Procesa una nueva muestra y actualiza la media exponencial.
     * @param nuevaMuestra Valor analógico actual (tensión o valor ADC).
     * @return Valor filtrado resultante.
     */
    float filtrar(float nuevaMuestra);

    /**
     * @brief Obtiene el valor actual almacenado en el filtro.
     */
    float obtenerValor() const;

    /**
     * @brief Reinicia el estado interno del filtro con un valor inicial.
     */
    void reiniciar(float valorInicial = 0.0f);

    /**
     * @brief Modifica dinámicamente el factor alfa.
     */
    void configurarAlpha(float nuevoAlpha);

private:
    float _alpha;
    float _valorFiltrado;
    bool  _inicializado;
};

#endif // SIGATOK_EMA_FILTER_H
