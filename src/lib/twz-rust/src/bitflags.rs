#[macro_export(local_inner_macros)]
macro_rules! bitflags {
    (
        $(#[$metas:meta])*
        $vis:vis struct $name:ident: $T:ty {
            $(
                $(#[$innermetas:meta])*
                const $flag:ident = $value:expr;
            )*
        }
    ) => {
        $(#[$metas])*
        #[repr(transparent)]
        #[derive(Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
        $vis struct $name {
            val: $T,
        }

        #[allow(dead_code)]
        impl $name {
            $(
                $(#[$innermetas])*
                pub const $flag: $name = $name { val: $value };
            )*

            /// Return set of all flags
            #[inline]
            pub const fn all() -> $name {
                $($name::$flag)|+
            }

            /// Return empty set of flags
            #[inline]
            pub const fn none() -> $name {
                $name { val : 0 }
            }

            /// Get raw bits value
            #[inline]
            pub const fn bits(&self) -> $T {
                self.val
            }

            /// Check if `self` contains all the bits in `other
            #[inline]
            pub const fn contains_all(&self, other: $name) -> bool {
                (self.val & other.val) == other.val
            }

            /// Check if `self` contains any the bits in `other
            #[inline]
            pub const fn contains_any(&self, other: $name) -> bool {
                self.val & other.val != 0
            }
        }

        impl core::ops::BitOr for $name {
            type Output = $name;

            /// Returns union of two bitflags
            fn bitor(self, other: $name) -> $name {
                $name { val: self.val | other.val }
            }
        }

        impl core::ops::BitAnd for $name {
            type Output = $name;

            /// Returns intersection of two bitflags
            fn bitand(self, other: $name) -> $name {
                $name { val: self.val & other.val }
            }
        }
    };
    () => {};
}
