/* TODO implement these */
pub(crate) fn has_clwb() -> bool {
    false
}

pub(crate) fn has_clflushopt() -> bool {
    false
}

pub(crate) const CACHE_LINE_SIZE: usize = 64;
