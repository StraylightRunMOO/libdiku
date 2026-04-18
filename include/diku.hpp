#ifndef DIKU_HPP
#define DIKU_HPP

#include "diku.h"
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace diku {

/* ------------------------------------------------------------------ */
/* String helper                                                      */
/* ------------------------------------------------------------------ */

inline std::string_view sv(const diku_string_t& s) noexcept {
    return s.str ? std::string_view(s.str, s.len) : std::string_view{};
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

class Area;
class AreaList;
class RoomRef;
class MobileRef;
class ItemRef;
class ExitRef;

/* ------------------------------------------------------------------ */
/* Context (RAII)                                                     */
/* ------------------------------------------------------------------ */

class Context {
    struct Deleter {
        void operator()(diku_context_t* p) const noexcept {
            diku_context_destroy(p);
        }
    };
    std::unique_ptr<diku_context_t, Deleter> ptr_;

public:
    Context() : ptr_(diku_context_create()) {}
    explicit Context(diku_context_t* raw) : ptr_(raw) {}

    diku_context_t* get() const noexcept { return ptr_.get(); }
    diku_context_t* operator->() const noexcept { return ptr_.get(); }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    void set_progress(diku_progress_cb_t cb, void* user = nullptr) noexcept {
        diku_context_set_progress(ptr_.get(), cb, user);
    }
};

/* ------------------------------------------------------------------ */
/* ExitRef                                                            */
/* ------------------------------------------------------------------ */

class ExitRef {
    exit_t* ptr_;
public:
    explicit ExitRef(exit_t* p) noexcept : ptr_(p) {}
    exit_t* get() const noexcept { return ptr_; }
    exit_t* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    int direction() const noexcept { return ptr_ ? ptr_->direction : -1; }
    std::string_view desc() const noexcept { return ptr_ ? sv(ptr_->desc) : std::string_view{}; }
    std::string_view keywords() const noexcept { return ptr_ ? sv(ptr_->keywords) : std::string_view{}; }
    int to_vnum() const noexcept { return ptr_ ? ptr_->to_vnum : -1; }
    room_t* to_room() const noexcept { return ptr_ ? ptr_->to_room : nullptr; }
};

/* ------------------------------------------------------------------ */
/* RoomRef                                                            */
/* ------------------------------------------------------------------ */

class RoomRef {
    room_t* ptr_;
public:
    explicit RoomRef(room_t* p) noexcept : ptr_(p) {}
    room_t* get() const noexcept { return ptr_; }
    room_t* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    int vnum() const noexcept { return ptr_ ? ptr_->vnum : -1; }
    std::string_view name() const noexcept { return ptr_ ? sv(ptr_->name) : std::string_view{}; }
    std::string_view desc() const noexcept { return ptr_ ? sv(ptr_->desc) : std::string_view{}; }
    int sector() const noexcept { return ptr_ ? ptr_->sector : -1; }

    const coord3d_t& coord() const noexcept { return ptr_ ? ptr_->coord : *static_cast<coord3d_t*>(nullptr); }
    bool coord_assigned() const noexcept { return ptr_ ? ptr_->coord_assigned : false; }

    ExitRef exit(int dir) const noexcept {
        if (!ptr_ || dir < 0 || dir >= DIKU_MAX_EXITS) return ExitRef(nullptr);
        return ExitRef(ptr_->exits[dir]);
    }

    int extra_desc_count() const noexcept { return ptr_ ? ptr_->extra_desc_count : 0; }
    int room_mobile_count() const noexcept { return ptr_ ? ptr_->room_mobile_count : 0; }
    int room_item_count() const noexcept { return ptr_ ? ptr_->room_item_count : 0; }
};

/* ------------------------------------------------------------------ */
/* MobileRef                                                          */
/* ------------------------------------------------------------------ */

class MobileRef {
    mobile_t* ptr_;
public:
    explicit MobileRef(mobile_t* p) noexcept : ptr_(p) {}
    mobile_t* get() const noexcept { return ptr_; }
    mobile_t* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    int vnum() const noexcept { return ptr_ ? ptr_->vnum : -1; }
    std::string_view name() const noexcept { return ptr_ ? sv(ptr_->name) : std::string_view{}; }
    std::string_view short_desc() const noexcept { return ptr_ ? sv(ptr_->short_desc) : std::string_view{}; }
    std::string_view long_desc() const noexcept { return ptr_ ? sv(ptr_->long_desc) : std::string_view{}; }
    int level() const noexcept { return ptr_ ? ptr_->level : 0; }
};

/* ------------------------------------------------------------------ */
/* ItemRef                                                            */
/* ------------------------------------------------------------------ */

class ItemRef {
    item_t* ptr_;
public:
    explicit ItemRef(item_t* p) noexcept : ptr_(p) {}
    item_t* get() const noexcept { return ptr_; }
    item_t* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    int vnum() const noexcept { return ptr_ ? ptr_->vnum : -1; }
    std::string_view name() const noexcept { return ptr_ ? sv(ptr_->name) : std::string_view{}; }
    std::string_view short_desc() const noexcept { return ptr_ ? sv(ptr_->short_desc) : std::string_view{}; }
    int type() const noexcept { return ptr_ ? ptr_->type : -1; }
    int weight() const noexcept { return ptr_ ? ptr_->weight : 0; }
    int cost() const noexcept { return ptr_ ? ptr_->cost : 0; }
};

/* ------------------------------------------------------------------ */
/* Area (non-owning view)                                             */
/* ------------------------------------------------------------------ */

class Area {
    area_t* ptr_;
public:
    Area() noexcept : ptr_(nullptr) {}
    explicit Area(area_t* p) noexcept : ptr_(p) {}

    area_t* get() const noexcept { return ptr_; }
    area_t* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    std::string_view name() const noexcept { return ptr_ ? sv(ptr_->name) : std::string_view{}; }
    std::string_view filename() const noexcept { return ptr_ ? sv(ptr_->filename) : std::string_view{}; }
    std::string_view builders() const noexcept { return ptr_ ? sv(ptr_->builders) : std::string_view{}; }

    int room_count() const noexcept { return ptr_ ? ptr_->room_count : 0; }
    int mobile_count() const noexcept { return ptr_ ? ptr_->mobile_count : 0; }
    int item_count() const noexcept { return ptr_ ? ptr_->item_count : 0; }

    Area next() const noexcept { return Area(ptr_ ? ptr_->next : nullptr); }

    /* Room array access */
    RoomRef room(int idx) const noexcept {
        if (!ptr_ || idx < 0 || idx >= ptr_->room_count) return RoomRef(nullptr);
        return RoomRef(&ptr_->rooms[idx]);
    }

    RoomRef room_by_vnum(int vnum) const noexcept {
        return RoomRef(ptr_ ? diku_find_room(ptr_, vnum) : nullptr);
    }

    MobileRef mobile_by_vnum(int vnum) const noexcept {
        return MobileRef(ptr_ ? diku_find_mobile(ptr_, vnum) : nullptr);
    }

    ItemRef item_by_vnum(int vnum) const noexcept {
        return ItemRef(ptr_ ? diku_find_item(ptr_, vnum) : nullptr);
    }

    /* Simple index-based iteration */
    RoomRef rooms_begin() const noexcept {
        return RoomRef(ptr_ && ptr_->room_count > 0 ? &ptr_->rooms[0] : nullptr);
    }
    RoomRef rooms_end() const noexcept {
        return RoomRef(ptr_ && ptr_->room_count > 0 ? &ptr_->rooms[ptr_->room_count] : nullptr);
    }

    MobileRef mobiles_begin() const noexcept {
        return MobileRef(ptr_ && ptr_->mobile_count > 0 ? &ptr_->mobiles[0] : nullptr);
    }
    MobileRef mobiles_end() const noexcept {
        return MobileRef(ptr_ && ptr_->mobile_count > 0 ? &ptr_->mobiles[ptr_->mobile_count] : nullptr);
    }

    ItemRef items_begin() const noexcept {
        return ItemRef(ptr_ && ptr_->item_count > 0 ? &ptr_->items[0] : nullptr);
    }
    ItemRef items_end() const noexcept {
        return ItemRef(ptr_ && ptr_->item_count > 0 ? &ptr_->items[ptr_->item_count] : nullptr);
    }
};

/* ------------------------------------------------------------------ */
/* AreaList (owning linked-list wrapper)                              */
/* ------------------------------------------------------------------ */

class AreaList {
    area_t* head_ = nullptr;

public:
    AreaList() noexcept = default;
    explicit AreaList(area_t* head) noexcept : head_(head) {}

    AreaList(AreaList&& other) noexcept : head_(other.head_) {
        other.head_ = nullptr;
    }
    AreaList& operator=(AreaList&& other) noexcept {
        if (this != &other) {
            if (head_) diku_free_all_areas(head_);
            head_ = other.head_;
            other.head_ = nullptr;
        }
        return *this;
    }

    ~AreaList() {
        if (head_) diku_free_all_areas(head_);
    }

    AreaList(const AreaList&) = delete;
    AreaList& operator=(const AreaList&) = delete;

    area_t* head() const noexcept { return head_; }
    bool empty() const noexcept { return head_ == nullptr; }
    explicit operator bool() const noexcept { return head_ != nullptr; }

    /* Release ownership (caller must free) */
    area_t* release() noexcept {
        area_t* tmp = head_;
        head_ = nullptr;
        return tmp;
    }

    /* Linked-list iterator */
    class iterator {
        area_t* ptr_;
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Area;
        using pointer = Area*;
        using reference = Area;
        using iterator_category = std::forward_iterator_tag;

        explicit iterator(area_t* p) noexcept : ptr_(p) {}
        iterator& operator++() noexcept { ptr_ = ptr_ ? ptr_->next : nullptr; return *this; }
        iterator operator++(int) noexcept { iterator tmp(*this); ++(*this); return tmp; }
        bool operator==(const iterator& o) const noexcept { return ptr_ == o.ptr_; }
        bool operator!=(const iterator& o) const noexcept { return ptr_ != o.ptr_; }
        Area operator*() const noexcept { return Area(ptr_); }
        Area operator->() const noexcept { return Area(ptr_); }
    };

    iterator begin() const noexcept { return iterator(head_); }
    iterator end() const noexcept { return iterator(nullptr); }
};

/* ------------------------------------------------------------------ */
/* Parsing functions                                                  */
/* ------------------------------------------------------------------ */

inline AreaList parse_file(Context& ctx, const char* path) {
    return AreaList(diku_parse_file(ctx.get(), path));
}

inline AreaList parse_lexer(Context& ctx, diku_lexer_t& lex, const char* filename) {
    return AreaList(diku_parse_lexer(&lex, filename));
}

inline AreaList parse_package(Context& ctx, const char* base_path) {
    return AreaList(diku_parse_package(ctx.get(), base_path));
}

inline AreaList parse_path(Context& ctx, const char* path) {
    return AreaList(diku_parse_path(ctx.get(), path));
}

inline AreaList load_folder_are(Context& ctx, const char* folder_path) {
    return AreaList(diku_load_folder_are(ctx.get(), folder_path));
}

inline AreaList load_folder_packages(Context& ctx, const char* folder_path) {
    return AreaList(diku_load_folder_packages(ctx.get(), folder_path));
}

/* ------------------------------------------------------------------ */
/* Graph & coordinates                                                */
/* ------------------------------------------------------------------ */

inline void resolve_graph(Context& ctx, AreaList& areas) {
    diku_resolve_graph_global(ctx.get(), areas.head());
}

inline void assign_coordinates(Context& ctx, Area& area, RoomRef root) {
    diku_assign_coordinates(ctx.get(), area.get(), root.get());
}

inline void assign_coordinates_all(Context& ctx, AreaList& areas) {
    diku_assign_coordinates_all(ctx.get(), areas.head());
}

inline void assign_coordinates_multi(Context& ctx, AreaList& areas) {
    diku_assign_coordinates_multi(ctx.get(), areas.head());
}

inline RoomRef find_central_room(Area& area) {
    return RoomRef(diku_find_central_room(area.get()));
}

inline void center_coordinates(Area& area) {
    diku_center_coordinates(area.get());
}

inline void get_coord_bounds(Area& area, int& min_x, int& max_x,
                             int& min_y, int& max_y, int& min_z, int& max_z) {
    diku_get_coord_bounds(area.get(), &min_x, &max_x, &min_y, &max_y, &min_z, &max_z);
}

/* ------------------------------------------------------------------ */
/* Stats & debug                                                      */
/* ------------------------------------------------------------------ */

inline void compute_totals(AreaList& areas, diku_totals_t& out) {
    diku_compute_totals(areas.head(), &out);
}

inline void print_totals(FILE* fp, const diku_totals_t& t, std::string_view source, AreaList& areas) {
    diku_print_totals(fp, &t, source.data(), areas.head());
}

inline void print_room_fp(FILE* fp, RoomRef room) {
    diku_print_room_fp(fp, room.get());
}

inline void print_mobile_fp(FILE* fp, MobileRef mob) {
    diku_print_mobile_fp(fp, mob.get());
}

inline void print_item_fp(FILE* fp, ItemRef item) {
    diku_print_item_fp(fp, item.get());
}

inline void print_area_info_fp(FILE* fp, Area area) {
    diku_print_area_info_fp(fp, area.get());
}

inline void print_graph_fp(FILE* fp, Area area) {
    diku_print_graph_fp(fp, area.get());
}

inline void print_coordinates_fp(FILE* fp, Area area) {
    diku_print_coordinates_fp(fp, area.get());
}

/* ------------------------------------------------------------------ */
/* Topology & utilities                                               */
/* ------------------------------------------------------------------ */

inline int graph_diameter(Context& ctx, Area& area) {
    return diku_graph_diameter(ctx.get(), area.get());
}

inline int check_exit_symmetry(Area& area, int& total, int& asymmetric) {
    return diku_check_exit_symmetry(area.get(), &total, &asymmetric);
}

inline void print_exit_symmetry(FILE* fp, Area& area) {
    diku_print_exit_symmetry(area.get());
    (void)fp;
}

inline diku_format_t detect_format(Area& area) {
    return diku_detect_format(area.get());
}

} // namespace diku

#endif /* DIKU_HPP */
