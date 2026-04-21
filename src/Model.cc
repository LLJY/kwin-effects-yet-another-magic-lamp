/*
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Own
#include "Model.h"

static inline std::chrono::milliseconds durationFraction(std::chrono::milliseconds duration, qreal fraction)
{
    return std::chrono::milliseconds(qMax(qRound(duration.count() * fraction), 1));
}

Model::Model(KWin::EffectWindow* window)
    : m_window(window)
{
}

static KWin::EffectWindow* findDock(const KWin::EffectWindow* client)
{
    const QList<KWin::EffectWindow*> windows = KWin::effects->stackingOrder();

    for (KWin::EffectWindow* window : windows) {
        if (!window->isDock())
            continue;

        if (!window->frameGeometry().intersects(client->iconGeometry()))
            continue;

        return window;
    }

    return nullptr;
}

static Direction realizeDirection(const KWin::EffectWindow* window)
{
    const KWin::EffectWindow* dock = findDock(window);

    Direction direction;
    QPointF screenDelta;

    if (dock) {
        const QRectF screenRect = KWin::effects->clientArea(KWin::ScreenArea, dock);

        if (dock->width() >= dock->height()) {
            if (qFuzzyIsNull(dock->y() - screenRect.y()))
                direction = Direction::Top;
            else
                direction = Direction::Bottom;
        } else {
            if (qFuzzyIsNull(dock->x() - screenRect.x()))
                direction = Direction::Left;
            else
                direction = Direction::Right;
        }

        screenDelta += screenRect.center();
    } else {
        // Perhaps the dock is hidden, deduce direction to the icon.
        const QRectF iconRect = window->iconGeometry();

        KWin::LogicalOutput* screen = KWin::effects->screenAt(iconRect.center().toPoint());

        QRectF screenRect;
        if (screen) {
            screenRect = KWin::effects->clientArea(KWin::ScreenArea, screen, KWin::effects->currentDesktop());
        } else {
            // Fallback when icon is off-screen (e.g. autohide panel)
            screenRect = KWin::effects->clientArea(KWin::ScreenArea, window);
        }
        const QRectF constrainedRect = screenRect.intersected(iconRect);

        if (qFuzzyIsNull(constrainedRect.left() - screenRect.left()))
            direction = Direction::Left;
        else if (qFuzzyIsNull(constrainedRect.top() - screenRect.top()))
            direction = Direction::Top;
        else if (qFuzzyIsNull(constrainedRect.right() - screenRect.right()))
            direction = Direction::Right;
        else
            direction = Direction::Bottom;

        screenDelta += screenRect.center();
    }

    const QRectF screenRect = KWin::effects->clientArea(KWin::ScreenArea, window);
    screenDelta -= screenRect.center();

    // Dock and window are on the same screen, no further adjustments are required.
    if (screenDelta.isNull())
        return direction;

    const int safetyMargin = 100;

    switch (direction) {
    case Direction::Top:
    case Direction::Bottom:
        // Approach the icon along horizontal direction.
        if (qAbs(screenDelta.x()) - qAbs(screenDelta.y()) > safetyMargin)
            return direction;

        // Approach the icon from bottom.
        if (screenDelta.y() < 0)
            return Direction::Top;

        // Approach the icon from top.
        return Direction::Bottom;

    case Direction::Left:
    case Direction::Right:
        // Approach the icon along vertical direction.
        if (qAbs(screenDelta.y()) - qAbs(screenDelta.x()) > safetyMargin)
            return direction;

        // Approach the icon from right side.
        if (screenDelta.x() < 0)
            return Direction::Left;

        // Approach the icon from left side.
        return Direction::Right;

    default:
        Q_UNREACHABLE();
    }
}

void Model::start(AnimationKind kind)
{
    m_kind = kind;
    m_done = false;

    if (m_timeLine.running()) {
        m_timeLine.toggleDirection();
        return;
    }

    m_direction = realizeDirection(m_window);
    m_bumpDistance = computeBumpDistance();
    m_shapeFactor = computeShapeFactor();

    switch (m_kind) {
    case AnimationKind::Minimize:
        if (!qFuzzyIsNull(m_bumpDistance)) {
            m_stage = AnimationStage::Bump;
            m_timeLine.reset();
            m_timeLine.setDirection(KWin::TimeLine::Forward);
            m_timeLine.setDuration(m_parameters.bumpDuration);
            m_timeLine.setEasingCurve(QEasingCurve::Linear);
            m_clip = false;
        } else {
            m_stage = AnimationStage::Stretch1;
            m_timeLine.reset();
            m_timeLine.setDirection(KWin::TimeLine::Forward);
            m_timeLine.setDuration(
                durationFraction(m_parameters.stretchDuration, m_shapeFactor));
            m_timeLine.setEasingCurve(QEasingCurve::Linear);
            m_clip = true;
        }
        break;

    case AnimationKind::Unminimize:
        m_stage = AnimationStage::Squash;
        m_timeLine.reset();
        m_timeLine.setDirection(KWin::TimeLine::Backward);
        m_timeLine.setDuration(m_parameters.squashDuration);
        m_timeLine.setEasingCurve(QEasingCurve::Linear);
        m_clip = true;
        break;

    default:
        Q_UNREACHABLE();
    }
}

void Model::advance(std::chrono::milliseconds presentTime)
{
    m_timeLine.advance(presentTime);
    if (!m_timeLine.done()) {
        return;
    }

    switch (m_kind) {
    case AnimationKind::Minimize:
        updateMinimizeStage();
        break;

    case AnimationKind::Unminimize:
        updateUnminimizeStage();
        break;

    default:
        Q_UNREACHABLE();
    }
}

void Model::updateMinimizeStage()
{
    switch (m_stage) {
    case AnimationStage::Bump:
        m_stage = AnimationStage::Stretch1;
        m_timeLine.reset();
        m_timeLine.setDirection(KWin::TimeLine::Forward);
        m_timeLine.setDuration(
            durationFraction(m_parameters.stretchDuration, m_shapeFactor));
        m_timeLine.setEasingCurve(QEasingCurve::Linear);
        m_clip = true;
        m_done = false;
        return;

    case AnimationStage::Stretch1:
    case AnimationStage::Stretch2:
        m_stage = AnimationStage::Squash;
        m_timeLine.reset();
        m_timeLine.setDirection(KWin::TimeLine::Forward);
        m_timeLine.setDuration(m_parameters.squashDuration);
        m_timeLine.setEasingCurve(QEasingCurve::Linear);
        m_clip = true;
        m_done = false;
        return;

    case AnimationStage::Squash:
        m_done = true;
        return;

    default:
        Q_UNREACHABLE();
    }
}

void Model::updateUnminimizeStage()
{
    switch (m_stage) {
    case AnimationStage::Bump:
        m_done = true;
        return;

    case AnimationStage::Stretch1:
        if (qFuzzyIsNull(m_bumpDistance)) {
            m_done = true;
            return;
        }
        m_stage = AnimationStage::Bump;
        m_timeLine.reset();
        m_timeLine.setDirection(KWin::TimeLine::Backward);
        m_timeLine.setDuration(m_parameters.bumpDuration);
        m_timeLine.setEasingCurve(QEasingCurve::Linear);
        m_clip = false;
        m_done = false;
        return;

    case AnimationStage::Stretch2:
        m_done = true;
        return;

    case AnimationStage::Squash:
        m_stage = AnimationStage::Stretch2;
        m_timeLine.reset();
        m_timeLine.setDirection(KWin::TimeLine::Backward);
        m_timeLine.setDuration(
            durationFraction(m_parameters.stretchDuration, m_shapeFactor));
        m_timeLine.setEasingCurve(QEasingCurve::Linear);
        m_clip = false;
        m_done = false;
        return;

    default:
        Q_UNREACHABLE();
    }
}

bool Model::done() const
{
    return m_done;
}

struct TransformParameters {
    QEasingCurve shapeCurve;
    Direction direction;
    qreal stretchProgress;
    qreal squashProgress;
    qreal bumpProgress;
    qreal bumpDistance;
};

// For Left/Right: quads in the same column share x-coords (quad[0].x(), quad[2].x()).
// For Top/Bottom: quads in the same row share y-coords (quad[0].y(), quad[2].y()).
// We cache the expensive shapeCurve.valueForProgress() across quads that share
// the same leading-edge coordinate, reducing curve evaluations from N_quads*2
// to N_rows*2 (or N_cols*2).

static void transformQuadsLeft(
    const KWin::EffectWindow* window,
    const TransformParameters& params,
    KWin::WindowQuadList& quads)
{
    const QRectF iconRect = window->iconGeometry();
    const QRectF windowRect = window->frameGeometry();

    const qreal distance = windowRect.right() - iconRect.right() + params.bumpDistance;
    const qreal invDistance = 1.0 / distance;
    const qreal squashShift = distance * params.squashProgress;
    const qreal bumpShift = params.bumpDistance * params.bumpProgress;
    const qreal iconY = iconRect.y();
    const qreal yScale = iconRect.height() / windowRect.height();
    const qreal winWidth = windowRect.width();
    const qreal winY = windowRect.y();

    qreal prevLeftX = -1.0, prevRightX = -1.0;
    qreal leftScale = 0.0, rightScale = 0.0;
    qreal leftOffset = 0.0, rightOffset = 0.0;
    qreal targetLeftOffset = 0.0, targetRightOffset = 0.0;

    for (int i = 0; i < quads.count(); ++i) {
        KWin::WindowQuad& quad = quads[i];

        if (quad[0].x() != prevLeftX || quad[2].x() != prevRightX) {
            prevLeftX = quad[0].x();
            prevRightX = quad[2].x();

            leftOffset = prevLeftX - squashShift;
            rightOffset = prevRightX - squashShift;

            leftScale = params.stretchProgress * params.shapeCurve.valueForProgress((winWidth - leftOffset) * invDistance);
            rightScale = params.stretchProgress * params.shapeCurve.valueForProgress((winWidth - rightOffset) * invDistance);

            targetLeftOffset = leftOffset + bumpShift;
            targetRightOffset = rightOffset + bumpShift;
        }

        const qreal targetTopLeftY = iconY + yScale * quad[0].y();
        const qreal targetBottomLeftY = iconY + yScale * quad[3].y();
        const qreal targetTopRightY = iconY + yScale * quad[1].y();
        const qreal targetBottomRightY = iconY + yScale * quad[2].y();

        quad[0].setY(quad[0].y() + leftScale * (targetTopLeftY - (winY + quad[0].y())));
        quad[3].setY(quad[3].y() + leftScale * (targetBottomLeftY - (winY + quad[3].y())));
        quad[1].setY(quad[1].y() + rightScale * (targetTopRightY - (winY + quad[1].y())));
        quad[2].setY(quad[2].y() + rightScale * (targetBottomRightY - (winY + quad[2].y())));

        quad[0].setX(targetLeftOffset);
        quad[3].setX(targetLeftOffset);
        quad[1].setX(targetRightOffset);
        quad[2].setX(targetRightOffset);
    }
}

static void transformQuadsTop(
    const KWin::EffectWindow* window,
    const TransformParameters& params,
    KWin::WindowQuadList& quads)
{
    const QRectF iconRect = window->iconGeometry();
    const QRectF windowRect = window->frameGeometry();

    const qreal distance = windowRect.bottom() - iconRect.bottom() + params.bumpDistance;
    const qreal invDistance = 1.0 / distance;
    const qreal squashShift = distance * params.squashProgress;
    const qreal bumpShift = params.bumpDistance * params.bumpProgress;
    const qreal iconX = iconRect.x();
    const qreal xScale = iconRect.width() / windowRect.width();
    const qreal winHeight = windowRect.height();
    const qreal winX = windowRect.x();

    qreal prevTopY = -1.0, prevBottomY = -1.0;
    qreal topScale = 0.0, bottomScale = 0.0;
    qreal topOffset = 0.0, bottomOffset = 0.0;
    qreal targetTopOffset = 0.0, targetBottomOffset = 0.0;

    for (int i = 0; i < quads.count(); ++i) {
        KWin::WindowQuad& quad = quads[i];

        if (quad[0].y() != prevTopY || quad[2].y() != prevBottomY) {
            prevTopY = quad[0].y();
            prevBottomY = quad[2].y();

            topOffset = prevTopY - squashShift;
            bottomOffset = prevBottomY - squashShift;

            topScale = params.stretchProgress * params.shapeCurve.valueForProgress((winHeight - topOffset) * invDistance);
            bottomScale = params.stretchProgress * params.shapeCurve.valueForProgress((winHeight - bottomOffset) * invDistance);

            targetTopOffset = topOffset + bumpShift;
            targetBottomOffset = bottomOffset + bumpShift;
        }

        const qreal targetTopLeftX = iconX + xScale * quad[0].x();
        const qreal targetTopRightX = iconX + xScale * quad[1].x();
        const qreal targetBottomRightX = iconX + xScale * quad[2].x();
        const qreal targetBottomLeftX = iconX + xScale * quad[3].x();

        quad[0].setX(quad[0].x() + topScale * (targetTopLeftX - (winX + quad[0].x())));
        quad[1].setX(quad[1].x() + topScale * (targetTopRightX - (winX + quad[1].x())));
        quad[2].setX(quad[2].x() + bottomScale * (targetBottomRightX - (winX + quad[2].x())));
        quad[3].setX(quad[3].x() + bottomScale * (targetBottomLeftX - (winX + quad[3].x())));

        quad[0].setY(targetTopOffset);
        quad[1].setY(targetTopOffset);
        quad[2].setY(targetBottomOffset);
        quad[3].setY(targetBottomOffset);
    }
}

static void transformQuadsRight(
    const KWin::EffectWindow* window,
    const TransformParameters& params,
    KWin::WindowQuadList& quads)
{
    const QRectF iconRect = window->iconGeometry();
    const QRectF windowRect = window->frameGeometry();

    const qreal distance = iconRect.left() - windowRect.left() + params.bumpDistance;
    const qreal invDistance = 1.0 / distance;
    const qreal squashShift = distance * params.squashProgress;
    const qreal bumpShift = params.bumpDistance * params.bumpProgress;
    const qreal iconY = iconRect.y();
    const qreal yScale = iconRect.height() / windowRect.height();
    const qreal winY = windowRect.y();

    qreal prevLeftX = -1.0, prevRightX = -1.0;
    qreal leftScale = 0.0, rightScale = 0.0;
    qreal leftOffset = 0.0, rightOffset = 0.0;
    qreal targetLeftOffset = 0.0, targetRightOffset = 0.0;

    for (int i = 0; i < quads.count(); ++i) {
        KWin::WindowQuad& quad = quads[i];

        if (quad[0].x() != prevLeftX || quad[2].x() != prevRightX) {
            prevLeftX = quad[0].x();
            prevRightX = quad[2].x();

            leftOffset = prevLeftX + squashShift;
            rightOffset = prevRightX + squashShift;

            leftScale = params.stretchProgress * params.shapeCurve.valueForProgress(leftOffset * invDistance);
            rightScale = params.stretchProgress * params.shapeCurve.valueForProgress(rightOffset * invDistance);

            targetLeftOffset = leftOffset - bumpShift;
            targetRightOffset = rightOffset - bumpShift;
        }

        const qreal targetTopLeftY = iconY + yScale * quad[0].y();
        const qreal targetBottomLeftY = iconY + yScale * quad[3].y();
        const qreal targetTopRightY = iconY + yScale * quad[1].y();
        const qreal targetBottomRightY = iconY + yScale * quad[2].y();

        quad[0].setY(quad[0].y() + leftScale * (targetTopLeftY - (winY + quad[0].y())));
        quad[3].setY(quad[3].y() + leftScale * (targetBottomLeftY - (winY + quad[3].y())));
        quad[1].setY(quad[1].y() + rightScale * (targetTopRightY - (winY + quad[1].y())));
        quad[2].setY(quad[2].y() + rightScale * (targetBottomRightY - (winY + quad[2].y())));

        quad[0].setX(targetLeftOffset);
        quad[3].setX(targetLeftOffset);
        quad[1].setX(targetRightOffset);
        quad[2].setX(targetRightOffset);
    }
}

static void transformQuadsBottom(
    const KWin::EffectWindow* window,
    const TransformParameters& params,
    KWin::WindowQuadList& quads)
{
    const QRectF iconRect = window->iconGeometry();
    const QRectF windowRect = window->frameGeometry();

    const qreal distance = iconRect.top() - windowRect.top() + params.bumpDistance;
    const qreal invDistance = 1.0 / distance;
    const qreal squashShift = distance * params.squashProgress;
    const qreal bumpShift = params.bumpDistance * params.bumpProgress;
    const qreal iconX = iconRect.x();
    const qreal xScale = iconRect.width() / windowRect.width();
    const qreal winX = windowRect.x();

    qreal prevTopY = -1.0, prevBottomY = -1.0;
    qreal topScale = 0.0, bottomScale = 0.0;
    qreal topOffset = 0.0, bottomOffset = 0.0;
    qreal targetTopOffset = 0.0, targetBottomOffset = 0.0;

    for (int i = 0; i < quads.count(); ++i) {
        KWin::WindowQuad& quad = quads[i];

        if (quad[0].y() != prevTopY || quad[2].y() != prevBottomY) {
            prevTopY = quad[0].y();
            prevBottomY = quad[2].y();

            topOffset = prevTopY + squashShift;
            bottomOffset = prevBottomY + squashShift;

            topScale = params.stretchProgress * params.shapeCurve.valueForProgress(topOffset * invDistance);
            bottomScale = params.stretchProgress * params.shapeCurve.valueForProgress(bottomOffset * invDistance);

            targetTopOffset = topOffset - bumpShift;
            targetBottomOffset = bottomOffset - bumpShift;
        }

        const qreal targetTopLeftX = iconX + xScale * quad[0].x();
        const qreal targetTopRightX = iconX + xScale * quad[1].x();
        const qreal targetBottomRightX = iconX + xScale * quad[2].x();
        const qreal targetBottomLeftX = iconX + xScale * quad[3].x();

        quad[0].setX(quad[0].x() + topScale * (targetTopLeftX - (winX + quad[0].x())));
        quad[1].setX(quad[1].x() + topScale * (targetTopRightX - (winX + quad[1].x())));
        quad[2].setX(quad[2].x() + bottomScale * (targetBottomRightX - (winX + quad[2].x())));
        quad[3].setX(quad[3].x() + bottomScale * (targetBottomLeftX - (winX + quad[3].x())));

        quad[0].setY(targetTopOffset);
        quad[1].setY(targetTopOffset);
        quad[2].setY(targetBottomOffset);
        quad[3].setY(targetBottomOffset);
    }
}

static void transformQuads(
    const KWin::EffectWindow* window,
    const TransformParameters& params,
    KWin::WindowQuadList& quads)
{
    switch (params.direction) {
    case Direction::Left:
        transformQuadsLeft(window, params, quads);
        break;

    case Direction::Top:
        transformQuadsTop(window, params, quads);
        break;

    case Direction::Right:
        transformQuadsRight(window, params, quads);
        break;

    case Direction::Bottom:
        transformQuadsBottom(window, params, quads);
        break;

    default:
        Q_UNREACHABLE();
    }
}

void Model::apply(KWin::WindowQuadList& quads) const
{
    switch (m_stage) {
    case AnimationStage::Bump:
        applyBump(quads);
        break;

    case AnimationStage::Stretch1:
        applyStretch1(quads);
        break;

    case AnimationStage::Stretch2:
        applyStretch2(quads);
        break;

    case AnimationStage::Squash:
        applySquash(quads);
        break;
    }
}

void Model::applyBump(KWin::WindowQuadList& quads) const
{
    TransformParameters params;
    params.shapeCurve = m_parameters.shapeCurve;
    params.direction = m_direction;
    params.squashProgress = 0.0;
    params.stretchProgress = 0.0;
    params.bumpProgress = m_timeLine.value();
    params.bumpDistance = m_bumpDistance;
    transformQuads(m_window, params, quads);
}

void Model::applyStretch1(KWin::WindowQuadList& quads) const
{
    TransformParameters params;
    params.shapeCurve = m_parameters.shapeCurve;
    params.direction = m_direction;
    params.squashProgress = 0.0;
    params.stretchProgress = m_shapeFactor * m_timeLine.value();
    params.bumpProgress = 1.0;
    params.bumpDistance = m_bumpDistance;
    transformQuads(m_window, params, quads);
}

void Model::applyStretch2(KWin::WindowQuadList& quads) const
{
    TransformParameters params;
    params.shapeCurve = m_parameters.shapeCurve;
    params.direction = m_direction;
    params.squashProgress = 0.0;
    params.stretchProgress = m_shapeFactor * m_timeLine.value();
    params.bumpProgress = params.stretchProgress;
    params.bumpDistance = m_bumpDistance;
    transformQuads(m_window, params, quads);
}

void Model::applySquash(KWin::WindowQuadList& quads) const
{
    TransformParameters params;
    params.shapeCurve = m_parameters.shapeCurve;
    params.direction = m_direction;
    params.squashProgress = m_timeLine.value();
    params.stretchProgress = qMin(m_shapeFactor + params.squashProgress, 1.0);
    params.bumpProgress = 1.0;
    params.bumpDistance = m_bumpDistance;
    transformQuads(m_window, params, quads);
}

Model::Parameters Model::parameters() const
{
    return m_parameters;
}

void Model::setParameters(const Parameters& parameters)
{
    m_parameters = parameters;
}

KWin::EffectWindow* Model::window() const
{
    return m_window;
}

Direction Model::direction() const
{
    return m_direction;
}

void Model::setWindow(KWin::EffectWindow* window)
{
    m_window = window;
}

bool Model::needsClip() const
{
    return m_clip;
}

QRect Model::clipRect() const
{
    const QRectF iconRect = m_window->iconGeometry();
    QRectF cr = m_window->expandedGeometry();

    switch (m_direction) {
    case Direction::Top:
        cr.translate(0, m_bumpDistance);
        cr.setTop(iconRect.top());
        cr.setLeft(qMin(iconRect.left(), cr.left()));
        cr.setRight(qMax(iconRect.right(), cr.right()));
        break;

    case Direction::Right:
        cr.translate(-m_bumpDistance, 0);
        cr.setRight(iconRect.right());
        cr.setTop(qMin(iconRect.top(), cr.top()));
        cr.setBottom(qMax(iconRect.bottom(), cr.bottom()));
        break;

    case Direction::Bottom:
        cr.translate(0, -m_bumpDistance);
        cr.setBottom(iconRect.bottom());
        cr.setLeft(qMin(iconRect.left(), cr.left()));
        cr.setRight(qMax(iconRect.right(), cr.right()));
        break;

    case Direction::Left:
        cr.translate(m_bumpDistance, 0);
        cr.setLeft(iconRect.left());
        cr.setTop(qMin(iconRect.top(), cr.top()));
        cr.setBottom(qMax(iconRect.bottom(), cr.bottom()));
        break;

    default:
        Q_UNREACHABLE();
    }

    return cr.toAlignedRect();
}

qreal Model::computeBumpDistance() const
{
    const QRectF windowRect = m_window->frameGeometry();
    const QRectF iconRect = m_window->iconGeometry();

    qreal bumpDistance = 0.0;
    switch (m_direction) {
    case Direction::Top:
        bumpDistance = std::max(qreal(0), iconRect.y() + iconRect.height() - windowRect.y());
        break;

    case Direction::Right:
        bumpDistance = std::max(qreal(0), windowRect.x() + windowRect.width() - iconRect.x());
        break;

    case Direction::Bottom:
        bumpDistance = std::max(qreal(0), windowRect.y() + windowRect.height() - iconRect.y());
        break;

    case Direction::Left:
        bumpDistance = std::max(qreal(0), iconRect.x() + iconRect.width() - windowRect.x());
        break;

    default:
        Q_UNREACHABLE();
    }

    bumpDistance += std::min(bumpDistance, m_parameters.bumpDistance);

    return bumpDistance;
}

qreal Model::computeShapeFactor() const
{
    const QRectF windowRect = m_window->frameGeometry();
    const QRectF iconRect = m_window->iconGeometry();

    qreal movingExtent = 0.0;
    qreal distanceToIcon = 0.0;
    switch (m_direction) {
    case Direction::Top:
        movingExtent = windowRect.height();
        distanceToIcon = windowRect.bottom() - iconRect.bottom() + m_bumpDistance;
        break;

    case Direction::Right:
        movingExtent = windowRect.width();
        distanceToIcon = iconRect.left() - windowRect.left() + m_bumpDistance;
        break;

    case Direction::Bottom:
        movingExtent = windowRect.height();
        distanceToIcon = iconRect.top() - windowRect.top() + m_bumpDistance;
        break;

    case Direction::Left:
        movingExtent = windowRect.width();
        distanceToIcon = windowRect.right() - iconRect.right() + m_bumpDistance;
        break;

    default:
        Q_UNREACHABLE();
    }

    if (distanceToIcon <= 0.0) {
        return 1.0;
    }

    const qreal minimumShapeFactor = movingExtent / distanceToIcon;
    return qMax(m_parameters.shapeFactor, minimumShapeFactor);
}
