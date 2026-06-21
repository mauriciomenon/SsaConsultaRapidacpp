-- Fixture schema for ssa_mem_stress CI smoke.
-- Creates ssa_table with the production column set and populates it with
-- synthetic rows so paging exercises multiple pages. Run with:
--   sqlite3 <db_path> < tools/generate_mem_fixture.sql

CREATE TABLE IF NOT EXISTS ssa_table (
    numero_ssa TEXT,
    situacao TEXT,
    derivada_de TEXT,
    localizacao_codigo TEXT,
    descricao_localizacao TEXT,
    equipamento TEXT,
    semana_cadastro INTEGER,
    data_cadastro TEXT,
    descricao_ssa TEXT,
    descricao_execucao TEXT,
    setor_emissor TEXT,
    setor_executor TEXT,
    solicitante TEXT,
    responsavel_programacao TEXT,
    responsavel_execucao TEXT,
    servico_origem TEXT,
    sistema_origem TEXT,
    arquivo_origem TEXT,
    data_planilha TEXT,
    grau_prioridade_emissao TEXT,
    grau_prioridade_planejamento TEXT,
    semana_programada INTEGER,
    semana_executada INTEGER,
    num_reprogramacoes INTEGER,
    total_deprogramacoes INTEGER
);

-- Generate 2000 synthetic rows via cross-join of two CTEs (45x45 = 2025,
-- capped at 2000). Each CTE stays well under SQLite's default recursion
-- limit of 1000. page-size 50 -> 40 pages for paging exercise.
WITH RECURSIVE lo(row_num) AS (
    SELECT 1
    UNION ALL
    SELECT row_num + 1 FROM lo WHERE row_num < 45
),
hi(row_num) AS (
    SELECT 1
    UNION ALL
    SELECT row_num + 1 FROM hi WHERE row_num < 45
),
product(idx) AS (
    SELECT lo.row_num, hi.row_num FROM lo, hi
)
INSERT INTO ssa_table (
    numero_ssa, situacao, derivada_de, localizacao_codigo,
    descricao_localizacao, equipamento, semana_cadastro, data_cadastro,
    descricao_ssa, descricao_execucao, setor_emissor, setor_executor,
    solicitante, responsavel_programacao, responsavel_execucao,
    servico_origem, sistema_origem, arquivo_origem, data_planilha,
    grau_prioridade_emissao, grau_prioridade_planejamento,
    semana_programada, semana_executada,
    num_reprogramacoes, total_deprogramacoes
)
SELECT
    '2025' || printf('%06d', idx),
    CASE idx % 4 WHEN 0 THEN 'APV' WHEN 1 THEN 'STE' WHEN 2 THEN 'SES' ELSE 'SCA' END,
    '',
    'LOC-' || (idx % 100),
    'Fixture location ' || idx,
    'EQ-' || (idx % 50),
    202501 + (idx % 4),
    '2025-01-' || printf('%02d', (idx % 28) + 1),
    'Fixture SSA description ' || idx,
    'Fixture execution text ' || idx,
    'SEM',
    CASE idx % 3 WHEN 0 THEN 'SMM' WHEN 1 THEN 'STE' ELSE 'OPR' END,
    'Solicitant ' || idx,
    'Planner ' || idx,
    'Executor ' || idx,
    'SAM',
    'SYS',
    'fixture.xlsx',
    '2025-01-01',
    'A', 'B',
    202502, 202503,
    idx % 5, idx % 5
FROM product
WHERE idx < 2000;
