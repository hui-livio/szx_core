.pragma library

// QVariantList crosses the C++/QML boundary as an indexable sequence rather
// than as a native JavaScript Array. Normalize it once at the shared toolkit
// boundary so application modules do not copy this compatibility logic.
function listValues(value) {
    if (value === undefined || value === null || typeof value !== "object")
        return [];

    const count = Number(value.length);
    if (!Number.isFinite(count) || count < 0 || Math.floor(count) !== count)
        return [];

    const values = [];
    for (let index = 0; index < count; ++index)
        values.push(value[index]);
    return values;
}

function chartSeriesValues(value) {
    return listValues(value).map(function(series) {
        if (!series || typeof series !== "object")
            return ({});
        const normalized = Object.assign({}, series);
        normalized.data = listValues(series.data);
        return normalized;
    });
}
