# Preserve the GOT offsets used by the remaining ARM modules.
s/_GLOBAL_OFFSET_TABLE_/nrio_got/g
s/([[:alnum:]_]+)\(GOT\)/__got_offset_\1/g
