namespace hareflow::detail {

template<typename Container, typename ElemSize> std::size_t BinaryBuffer::serialized_size(Container&& container, ElemSize&& elem_size)
{
    std::size_t size = sizeof(std::int32_t);
    auto        end  = std::cend(container);
    for (auto it = std::cbegin(container); it != end; ++it) {
        size += elem_size(*it);
    }
    return size;
}

template<typename Container, typename ReadElem> void BinaryBuffer::read_array(Container&& container, ReadElem&& read_elem)
{
    std::int32_t length = read_int();
    if (length <= 0) {
        return;
    }
    auto it = std::inserter(container, std::end(container));
    for (int i = 0; i < length; ++i) {
        *it = read_elem(*this);
    }
}

template<typename Container, typename WriteElem> void BinaryBuffer::write_array(Container&& container, WriteElem&& write_elem)
{
    auto it    = std::cbegin(container);
    auto end   = std::cend(container);
    auto count = std::distance(it, end);
    if (count > std::numeric_limits<std::int32_t>::max()) {
        throw InvalidInputException(fmt::format("Maximum array size exceeded ({})", std::numeric_limits<std::int32_t>::max()));
    }
    write_int(static_cast<std::int32_t>(count));
    for (; it != end; ++it) {
        write_elem(*this, *it);
    }
}

}  // namespace hareflow::detail