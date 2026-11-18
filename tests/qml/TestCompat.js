.pragma library

// QObject list properties were not full JavaScript arrays on the Ubuntu
// 24.04 Qt floor. Compare list entries directly instead of relying on
// Array.prototype.indexOf().
function containsIdentity(sequence, wanted) {
    for (var index = 0; index < sequence.length; ++index) {
        if (sequence[index] === wanted) {
            return true
        }
    }
    return false
}

function readJson(url) {
    var request = new XMLHttpRequest()
    request.open("GET", url, false)
    request.send()
    if (request.status !== 0 && request.status !== 200) {
        throw new Error("Could not read JSON resource " + url + ": " + request.status)
    }
    return JSON.parse(request.responseText)
}

function _propertyNames(object) {
    var names = []
    for (var name in object) {
        if (name !== "parent" && name !== "children" && name !== "data") {
            names.push(name)
        }
    }
    return names.sort()
}

function _orderedCandidates(object) {
    var candidates = []
    var ordinal = 0

    function append(value, source, propertyName) {
        // Only QObjects have objectName. Expanding QColor/font/anchor value
        // wrappers and plain facade snapshots walks data, not the UI graph.
        if (!value || typeof value.objectName !== "string") {
            return
        }
        candidates.push({
            object: value,
            source: source,
            propertyName: propertyName,
            ordinal: ordinal
        })
        ++ordinal
    }

    if (object.children) {
        for (var childIndex = 0; childIndex < object.children.length; ++childIndex) {
            append(object.children[childIndex], 0, "")
        }
    }
    if (object.data) {
        for (var dataIndex = 0; dataIndex < object.data.length; ++dataIndex) {
            append(object.data[dataIndex], 1, "")
        }
    }
    _propertyNames(object).forEach(function(name) {
        var value
        try {
            value = object[name]
        } catch (error) {
            return
        }
        if (!value || typeof value !== "object") {
            return
        }
        if (value instanceof Array) {
            for (var valueIndex = 0; valueIndex < value.length; ++valueIndex) {
                append(value[valueIndex], 2, name)
            }
            return
        }
        append(value, 2, name)
    })

    return candidates.sort(function(left, right) {
        if (left.source !== right.source) {
            return left.source - right.source
        }
        var propertyOrder = left.propertyName.localeCompare(right.propertyName)
        if (propertyOrder !== 0) {
            return propertyOrder
        }
        return left.ordinal - right.ordinal
    })
}

function semanticIds(root) {
    var ids = []
    walkQObjectGraph(root).forEach(function(object) {
        if (object.objectName && object.objectName.indexOf("starlight.") === 0) {
            ids.push(object.objectName.substring("starlight.".length))
        }
    })
    return ids.sort()
}

function walkQObjectGraph(root) {
    var visited = new Set()
    var result = []

    function visit(object) {
        if (!object || visited.has(object)) {
            return
        }
        visited.add(object)
        result.push(object)

        _orderedCandidates(object).forEach(function(candidate) {
            visit(candidate.object)
        })
    }

    visit(root)
    return result
}

function hasMethod(object, name) {
    return !!object && typeof object[name] === "function"
}

function countByType(root, typeName) {
    return walkQObjectGraph(root).filter(function(object) {
        return object && object.toString && object.toString().indexOf(typeName) !== -1
    }).length
}
