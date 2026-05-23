#include "Annotation.h"

#include <type_traits>
#include <objidl.h>
#include <gdiplus.h>

int GetLastVisibleAnnotationIndex(const std::vector<AnnotationObject>& annotationObjects)
{
    for (int index = static_cast<int>(annotationObjects.size()) - 1; index >= 0; --index)
    {
        if (annotationObjects[static_cast<size_t>(index)].isVisible)
        {
            return index;
        }
    }

    return -1;
}

void MoveAnnotation(AnnotationObject& annotationObject, LONG dx, LONG dy)
{
    std::visit(
        [dx, dy](auto& annotation)
        {
            using Annotation = std::decay_t<decltype(annotation)>;
            if constexpr (std::is_same_v<Annotation, ArrowAnnotation>)
            {
                MoveArrow(annotation, dx, dy);
            }
        },
        annotationObject.value);
}

std::vector<AnnotationObject> MoveAnnotations(const std::vector<AnnotationObject>& annotationObjects, LONG dx, LONG dy)
{
    std::vector<AnnotationObject> movedAnnotationObjects = annotationObjects;
    for (AnnotationObject& annotationObject : movedAnnotationObjects)
    {
        MoveAnnotation(annotationObject, dx, dy);
    }

    return movedAnnotationObjects;
}

void DrawAnnotation(Gdiplus::Graphics& graphics, const AnnotationObject& annotationObject, int offsetX, int offsetY)
{
    if (!annotationObject.isVisible)
    {
        return;
    }

    std::visit(
        [&graphics, offsetX, offsetY](const auto& annotation)
        {
            using Annotation = std::decay_t<decltype(annotation)>;
            if constexpr (std::is_same_v<Annotation, ArrowAnnotation>)
            {
                DrawArrow(graphics, annotation, offsetX, offsetY);
            }
        },
        annotationObject.value);
}

void DrawAnnotations(
    Gdiplus::Graphics& graphics,
    const std::vector<AnnotationObject>& annotationObjects,
    int offsetX,
    int offsetY)
{
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for (const AnnotationObject& annotationObject : annotationObjects)
    {
        DrawAnnotation(graphics, annotationObject, offsetX, offsetY);
    }
}
