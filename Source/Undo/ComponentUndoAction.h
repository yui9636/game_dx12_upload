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
            // 必要なら Dirty フラグを立てる（Transform 等の場合）
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
    // TransformComponent などの Dirty フラグを自動で立てるヘルパー
    void SetDirtyIfPossible(void* comp) {
        // コンパイル時に TransformComponent かどうか判定して処理
        // （実際にはオーバーロード等で実装）
    }
};