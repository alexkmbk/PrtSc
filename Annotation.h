#pragma once

#include "ArrowAnnotation.h"
#include "TextAnnotation.h"

#include <vector>
#include <variant>
#include <windows.h>

namespace Gdiplus
{
class Graphics;
}

using AnnotationValue = std::variant<ArrowAnnotation, TextAnnotation>;

struct AnnotationObject
{
    AnnotationValue value;
    bool isVisible = true;
};

int GetLastVisibleAnnotationIndex(const std::vector<AnnotationObject>& annotationObjects);
void MoveAnnotation(AnnotationObject& annotationObject, LONG dx, LONG dy);
std::vector<AnnotationObject> MoveAnnotations(const std::vector<AnnotationObject>& annotationObjects, LONG dx, LONG dy);
void DrawAnnotation(Gdiplus::Graphics& graphics, const AnnotationObject& annotationObject, int offsetX = 0, int offsetY = 0);
void DrawAnnotations(
    Gdiplus::Graphics& graphics,
    const std::vector<AnnotationObject>& annotationObjects,
    int offsetX = 0,
    int offsetY = 0);
