template<typename T>
class ComponentUndoAction : public IUndoAction {
    EntityID m_target;
    T m_oldState;
    T m_newState;

public:
    ComponentUndoAction(EntityID entity, const T& oldState, const T& newState)
        : m_target(entity), m_oldState(oldState), m_newState(newState) {
    }

    void Undo(Registry& registry) override {
        auto* comp = registry.GetComponent<T>(m_target);
        if (comp) {
            *comp = m_oldState;
            SetDirtyIfPossible(comp);
        }
    }

    void Redo(Registry& registry) override {
        auto* comp = registry.GetComponent<T>(m_target);
        if (comp) {
            *comp = m_newState;
            SetDirtyIfPossible(comp);
        }
    }

private:
    void SetDirtyIfPossible(void* comp) {
    }
};
