#pragma once

// Looks up the human-readable description for a DTC (e.g. "P0133") in the
// flash-resident table generated from src/data/Mustang_DTC.csv (see
// src/system/dtc_table.h / src/scripts/csv_to_dtc_table.py). Returns nullptr
// for codes not in the table (manufacturer-specific or otherwise unmapped).
const char* lookupDtcDescription(const char* code);
