#include "presentation/BrowsePageLifecycleCoordinator.h"

#include <utility>

namespace ssa::presentation {

    BrowsePageLifecycleCoordinator::BrowsePageLifecycleCoordinator(BrowseQueryState& queryState,
                                                                   SsaTableModel& tableModel,
                                                                   StatusViewModel& status,
                                                                   QObject* parent)
        : QObject(parent), queryState_(queryState), tableModel_(tableModel), status_(status) {}

    void BrowsePageLifecycleCoordinator::markRequestStarted() {
        status_.setLoading(true);
        status_.setError({});
        status_.setMessage("Consultando dados...");
    }

    void BrowsePageLifecycleCoordinator::markRequestCanceling() {
        status_.setLoading(true);
        status_.setError({});
        status_.setMessage("Cancelando consulta...");
    }

    void BrowsePageLifecycleCoordinator::markRequestReplaced() {
        status_.setLoading(true);
        status_.setMessage("Atualizando consulta...");
    }

    void
    BrowsePageLifecycleCoordinator::markRequestSucceeded(PageQueryResult result,
                                                         const domain::SsaPageRequest& request) {
        queryState_.applyPageResult(result.page, result.totalRowsAll);
        tableModel_.setPage(std::move(result.page), request.visibleColumns,
                            std::move(result.displayColumns), std::move(result.displayValues));
        status_.setQueryComplete(queryState_.totalRows(), queryState_.totalRowsAll(),
                                 queryState_.pageNumber(), queryState_.pageCount());
        status_.setLoading(false);
    }

    void BrowsePageLifecycleCoordinator::markRequestCanceled() {
        status_.setLoading(false);
        status_.setMessage("Consulta cancelada");
    }

    void BrowsePageLifecycleCoordinator::markRequestFailed(const QString& message) {
        status_.setError(message);
        status_.setMessage("Falha ao consultar dados; tabela ainda mostra o resultado anterior");
        status_.setLoading(false);
    }

} // namespace ssa::presentation
