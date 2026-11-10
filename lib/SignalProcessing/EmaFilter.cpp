/**
 * @file EmaFilter.cpp
 * @brief Implementación del filtro EMA.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "EmaFilter.h"

EmaFilter::EmaFilter(float alpha)
    : _alpha(alpha), _valorFiltrado(0.0f), _inicializado(false) {
    if (_alpha <= 0.0f || _alpha > 1.0f) {
        _alpha = 0.15f; // Valor por defecto ante parámetros inválidos
    }
}

float EmaFilter::filtrar(float nuevaMuestra) {
    if (!_inicializado) {
        _valorFiltrado = nuevaMuestra;
        _inicializado = true;
        return _valorFiltrado;
    }

    // Ecuación en diferencias de primer orden: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    _valorFiltrado = (_alpha * nuevaMuestra) + ((1.0f - _alpha) * _valorFiltrado);
    return _valorFiltrado;
}

float EmaFilter::obtenerValor() const {
    return _valorFiltrado;
}

void EmaFilter::reiniciar(float valorInicial) {
    _valorFiltrado = valorInicial;
    _inicializado = false;
}

void EmaFilter::configurarAlpha(float nuevoAlpha) {
    if (nuevoAlpha > 0.0f && nuevoAlpha <= 1.0f) {
        _alpha = nuevoAlpha;
    }
}
