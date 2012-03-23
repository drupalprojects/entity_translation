<?php

/**
 * @file
 * Node translation handler for the entity translation module.
 */


/**
 * Node translation handler.
 *
 * Overrides default behaviours for Node properties.
 */
class EntityTranslationNodeHandler extends EntityTranslationDefaultHandler {

  public function __construct($entity_type, $entity_info, $entity, $entity_id) {
    parent::__construct('node', $entity_info, $entity, $entity_id);
  }

  public function isRevision() {
    return !empty($this->entity->revision);
  }

  public function getAccess($op) {
    return node_access($op, $this->entity);
  }

  /**
   * Convert the translation update status fieldset into a vartical tab.
   */
  public function entityForm(&$form, &$form_state) {
    parent::entityForm($form, $form_state);

    // Move the translation fieldset to a vertical tab.
    if (isset($form['translation'])) {
      $form['translation'] += array(
        '#group' => 'additional_settings',
        '#weight' => 100,
        '#attached' => array(
          'js' => array(drupal_get_path('module', 'entity_translation') . '/entity_translation.node-form.js'),
        ),
      );

      if (!$this->isTranslationForm()) {
        $form['translation']['name']['#access'] = FALSE;
        $form['translation']['created']['#access'] = FALSE;
      }
    }
  }

  public function entityFormSubmit($form, &$form_state) {
    if (!isset($form_state['values']['translation'])) {
      $form_state['values']['translation'] = array('status' => $form_state['values']['status']);
    }
    $values = &$form_state['values']['translation'];

    if (!$this->isTranslationForm()) {
      // Inherit entity authoring information for the original values.
      $values['name'] = $form_state['values']['name'];
      if (!empty($form_state['values']['date'])) {
        $values['created'] = $form_state['values']['date'];
      }
    }

    parent::entityFormSubmit($form, $form_state);
  }

  protected function entityFormTitle() {
    $type_name = node_type_get_name($this->entity);
    return t('<em>Edit @type</em> @title', array('@type' => $type_name, '@title' => $this->getLabel()));
  }

  protected function getStatus() {
    return (boolean) $this->entity->status;
  }
}
