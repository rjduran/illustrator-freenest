#pragma once
namespace nestai {
enum class Units { Inches, Millimeters, Centimeters, Points, Picas, Pixels, Q, FeetInches, Meters, Yards, Feet };
inline constexpr Units UnitChoices[]={Units::Inches,Units::Millimeters,Units::Centimeters,Units::Points,Units::Pixels,Units::Meters,Units::Feet};
inline double PointsPerUnit(Units units) {
    switch(units) {
        case Units::Inches:case Units::FeetInches:return 72;
        case Units::Millimeters:return 72/25.4;
        case Units::Centimeters:return 72/2.54;
        case Units::Picas:return 12;
        case Units::Q:return 72/101.6;
        case Units::Meters:return 72000/25.4;
        case Units::Yards:return 72*36;
        case Units::Feet:return 72*12;
        default:return 1; // Illustrator points and pixels share the same document scale.
    }
}
inline const char* UnitSymbol(Units units) {
    switch(units) {
        case Units::Inches:case Units::FeetInches:return "in";
        case Units::Millimeters:return "mm";case Units::Centimeters:return "cm";
        case Units::Picas:return "pc";case Units::Pixels:return "px";case Units::Q:return "Q";
        case Units::Meters:return "m";case Units::Yards:return "yd";case Units::Feet:return "ft";
        default:return "pt";
    }
}
inline const char* UnitName(Units units) {return units==Units::FeetInches?"ft/in (decimal inches)":UnitSymbol(units);}
inline double ConvertUnits(double value,Units from,Units to) {return value*PointsPerUnit(from)/PointsPerUnit(to);}
}
