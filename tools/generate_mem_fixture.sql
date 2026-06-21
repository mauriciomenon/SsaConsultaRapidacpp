-- Fixture schema for ssa_mem_stress CI smoke.
-- Creates ssa_table with the production column set and populates it with
-- synthetic rows so paging exercises multiple pages. Run with:
--   sqlite3 <db_path> < tools/generate_mem_fixture.sql

CREATE TABLE ssa_table (
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
    total_de_reprogramacoes INTEGER
);

-- Generate 2000 synthetic rows via recursive CTE. page-size 50 -> 40 pages,
-- enough to exercise paging without bloating the CI runner.
WITH RECURSIVE sequence(row_num) AS (
    SELECT 1
    UNION ALL
    SELECT row_num + 1 FROM sequence WHERE row_num < 2000
)
INSERT INTO ssa_table (
    numero_ssa, situacao, derivada_de, localizacao_codigo,
    descricao_localizacao, equipamento, semana_cadastro, data_cadastro,
    descricao_ssa, descricao_execucao, setor_emissor, setor_executor,
    solicitante, responsavel_programacao, responsavel_execucao,
    servico_origem, sistema_origem, arquivo_origem, data_planilha,
    grau_prioridade_emissao, grau_prioridade_planejamento,
    semana_programada, semana_executada,
    num_reprogramacoes, total_de_reprogramacoes
)
SELECT
    '2025' || printf('%06d', row_num),
    CASE row_num % 4 WHEN 0 THEN 'APV' WHEN 1 THEN 'STE' WHEN 2 THEN 'SES' ELSE 'SCA' END,
    '',
    'LOC-' || (row_num % 100),
    'Fixture location ' || row_num,
    'EQ-' || (row_num % 50),
    202501 + (row_num % 4),
    '2025-01-' || printf('%02d', (row_num % 28) + 1),
    'Fixture SSA description ' || row_num,
    'Fixture execution text ' || row_num,
    'SEM',
    CASE row_num % 3 WHEN 0 THEN 'SMM' WHEN 1 THEN 'STE' ELSE 'OPR' END,
    'Solicitant ' || row_num,
    'Planner ' || row_num,
    'Executor ' || row_num,
    'SAM',
    'SYS',
    'fixture.xlsx',
    '2025-01-01',
    'A', 'B',
    202502, 202503,
    row_num % 5, row_num % 5
FROM sequence;
