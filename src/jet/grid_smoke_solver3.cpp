// Copyright (c) 2016 Doyub Kim

#include <pch.h>
#include <jet/grid_smoke_solver3.h>
#include <algorithm>

using namespace jet;

GridSmokeSolver3::GridSmokeSolver3() {
    auto grids = gridSystemData();

    _smokeDensityDataId = grids->addAdvectableScalarData(
        CellCenteredScalarGrid3::builder(), 0.0);
    _temperatureDataId = grids->addAdvectableScalarData(
        CellCenteredScalarGrid3::builder(), 0.0);
}

GridSmokeSolver3::~GridSmokeSolver3() {
}

double GridSmokeSolver3::smokeDiffusionCoefficient() const {
    return _smokeDiffusionCoefficient;
}

void GridSmokeSolver3::setSmokeDiffusionCoefficient(double newValue) {
    _smokeDiffusionCoefficient = std::max(newValue, 0.0);
}

double GridSmokeSolver3::temperatureDiffusionCoefficient() const {
    return _temperatureDiffusionCoefficient;
}

void GridSmokeSolver3::setTemperatureDiffusionCoefficient(double newValue) {
    _temperatureDiffusionCoefficient = std::max(newValue, 0.0);
}

double GridSmokeSolver3::buoyancySmokeDensityFactor() const {
    return _buoyancySmokeDensityFactor;
}

void GridSmokeSolver3::setBuoyancySmokeDensityFactor(double newValue) {
    _buoyancySmokeDensityFactor = newValue;
}

double GridSmokeSolver3::buoyancyTemperatureFactor() const {
    return _buoyancyTemperatureFactor;
}

void GridSmokeSolver3::setBuoyancyTemperatureFactor(double newValue) {
    _buoyancyTemperatureFactor = newValue;
}

ScalarGrid3Ptr GridSmokeSolver3::smokeDensity() const {
    return gridSystemData()->advectableScalarDataAt(_smokeDensityDataId);
}

ScalarGrid3Ptr GridSmokeSolver3::temperature() const {
    return gridSystemData()->advectableScalarDataAt(_temperatureDataId);
}

void GridSmokeSolver3::onEndAdvanceTimeStep(double timeIntervalInSeconds) {
    computeDiffusion(timeIntervalInSeconds);
}

void GridSmokeSolver3::computeExternalForces(double timeIntervalInSeconds) {
    computeBuoyancyForce(timeIntervalInSeconds);
}

void GridSmokeSolver3::computeDiffusion(double timeIntervalInSeconds) {
    if (diffusionSolver() != nullptr) {
        if (_smokeDiffusionCoefficient > kEpsilonD) {
            auto den = smokeDensity();
            auto den0 = std::dynamic_pointer_cast<CellCenteredScalarGrid3>(
                den->clone());

            diffusionSolver()->solve(
                *den0,
                _smokeDiffusionCoefficient,
                timeIntervalInSeconds,
                den0.get(),
                colliderSdf());
            extrapolateIntoCollider(den.get());
        }

        if (_temperatureDiffusionCoefficient > kEpsilonD) {
            auto temp = smokeDensity();
            auto temp0 = std::dynamic_pointer_cast<CellCenteredScalarGrid3>(
                temp->clone());

            diffusionSolver()->solve(
                *temp0,
                _temperatureDiffusionCoefficient,
                timeIntervalInSeconds,
                temp.get(),
                colliderSdf());
            extrapolateIntoCollider(temp.get());
        }
    }
}

void GridSmokeSolver3::computeBuoyancyForce(double timeIntervalInSeconds) {
    auto grids = gridSystemData();
    auto vel = grids->velocity();

    Vector3D up(0, 1, 0);
    if (gravity().lengthSquared() > kEpsilonD) {
        up = -gravity().normalized();
    }

    if (std::abs(_buoyancySmokeDensityFactor) > kEpsilonD ||
        std::abs(_buoyancyTemperatureFactor) > kEpsilonD) {
        auto den = smokeDensity();
        auto temp = temperature();

        double tAmb = 0.0;
        temp->forEachCellIndex([&](size_t i, size_t j, size_t k) {
            tAmb += (*temp)(i, j, k);
        });
        tAmb /= static_cast<double>(
            temp->resolution().x * temp->resolution().y * temp->resolution().z);

        auto u = vel->uAccessor();
        auto v = vel->vAccessor();
        auto w = vel->wAccessor();
        auto uPos = vel->uPosition();
        auto vPos = vel->vPosition();
        auto wPos = vel->wPosition();

        if (std::abs(up.x) > kEpsilonD) {
            vel->parallelForEachUIndex([&](size_t i, size_t j, size_t k) {
                Vector3D pt = uPos(i, j, k);
                double fBuoy
                    = _buoyancySmokeDensityFactor * den->sample(pt)
                    + _buoyancyTemperatureFactor * (temp->sample(pt) - tAmb);
                u(i, j, k) += timeIntervalInSeconds * fBuoy * up.x;
            });
        }

        if (std::abs(up.y) > kEpsilonD) {
            vel->parallelForEachVIndex([&](size_t i, size_t j, size_t k) {
                Vector3D pt = vPos(i, j, k);
                double fBuoy
                    = _buoyancySmokeDensityFactor * den->sample(pt)
                    + _buoyancyTemperatureFactor * (temp->sample(pt) - tAmb);
                v(i, j, k) += timeIntervalInSeconds * fBuoy * up.y;
            });
        }

        if (std::abs(up.z) > kEpsilonD) {
            vel->parallelForEachWIndex([&](size_t i, size_t j, size_t k) {
                Vector3D pt = wPos(i, j, k);
                double fBuoy
                    = _buoyancySmokeDensityFactor * den->sample(pt)
                    + _buoyancyTemperatureFactor * (temp->sample(pt) - tAmb);
                w(i, j, k) += timeIntervalInSeconds * fBuoy * up.z;
            });
        }

        applyBoundaryCondition();
    }
}