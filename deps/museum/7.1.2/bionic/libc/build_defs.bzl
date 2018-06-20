def flattened_glob(prefix, glob_pattern, exclude = []):
    prefix_len = len(prefix)
    return { x[prefix_len:]: x for x in native.glob([prefix + glob_pattern], exclude = [prefix + e for e in exclude]) }

def merge_dicts(*dicts):
    result = {}
    for dictionary in dicts:
        result.update(dictionary)
    return result
