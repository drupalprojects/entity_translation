<?php

/**
 * @file
 * The taxonomy specific translation functions and hook implementations.
 */

/**
 * Returns whether the given taxonomy vocabulary has support for translations.
 *
 * @return bool
 *   TRUE if translation is enabled, FALSE otherwise.
 */
function entity_translation_taxonomy_term_enabled_vocabulary($vocabulary_name) {
  $info = variable_get('entity_translation_taxonomy', array());
  return !empty($info[$vocabulary_name]);
}

/**
 * Taxonomy-term-specific menu alterations.
 */
function entity_translation_taxonomy_term_menu_alter(&$items, $backup) {
  if (isset($backup['taxonomy_term'])) {
    $item = $backup['taxonomy_term'];
    // Preserve the menu router item defined by other modules.
    $callback['page callback'] = $item['page callback'];
    $callback['file'] = $item['file'];
    $callback['module'] = $item['module'];
    $access_arguments = array_merge(array(2, $item['access callback']), $item['access arguments']);
    $page_arguments = array_merge(array('taxonomy_term', 2, $callback), $item['page arguments']);
  }
  else {
    $access_arguments = array(2);
    $page_arguments = array('taxonomy_term', 2);
  }

  $items['taxonomy/term/%taxonomy_term/translate']['page callback'] = 'entity_translation_overview';
  $items['taxonomy/term/%taxonomy_term/translate']['page arguments'] = $page_arguments;
  $items['taxonomy/term/%taxonomy_term/translate']['access arguments'] = $access_arguments;
  $items['taxonomy/term/%taxonomy_term/translate']['access callback'] = 'entity_translation_taxonomy_term_tab_access';
  $items['taxonomy/term/%taxonomy_term/translate']['file'] = 'entity_translation.admin.inc';
  $items['taxonomy/term/%taxonomy_term/translate']['module'] = 'entity_translation';

  // Delete translation callback.
  $items['taxonomy/term/%taxonomy_term/translate/delete/%entity_translation_language']['access arguments'] = $access_arguments;
}

/**
 * Taxonomy term specific access callback.
 */
function entity_translation_taxonomy_term_tab_access() {
  $args = func_get_args();
  $term = array_shift($args);
  if (entity_translation_enabled('taxonomy_term', $term)) {
    return entity_translation_tab_access('taxonomy_term', $term);
  }
  else {
    $function = array_shift($args);
    return $function ? call_user_func_array($function, $args) : FALSE;
  }
}

/**
 * Implements hook_form_FORM_ID_alter()
 */
function entity_translation_form_taxonomy_form_vocabulary_alter(&$form, &$form_state) {
  if (entity_translation_enabled('taxonomy_term')) {
    $name = $form_state['vocabulary']->machine_name;
    if (isset($form['i18n_translation']['i18n_mode'])) {
      $args = array('!url' => url('admin/config/regional/entity_translation'));
      $form['i18n_translation']['i18n_mode']['#options'][I18N_MODE_ENTITY_TRANSLATION] = t('Field translation. Term fields will be translated through the <a href="!url">Entity translation</a> module.', $args);
      if (entity_translation_enabled_bundle('taxonomy_term', $name)) {
        $form['i18n_translation']['i18n_mode']['#default_value'] = I18N_MODE_ENTITY_TRANSLATION;
      }
    }
    else {
      $form['entity_translation_taxonomy'] = array(
        '#title' => t('Enable field translation'),
        '#type' => 'checkbox',
        '#prefix' => '<label>' . t('Translation') . '</label>',
        '#default_value' => entity_translation_enabled('taxonomy_term', $name),
      );
    }
    $form['#submit'][] = 'entity_translation_form_taxonomy_form_vocabulary_submit';
  }
}

/**
 * Submit handler for the taxonomy vocabulary form.
 */
function entity_translation_form_taxonomy_form_vocabulary_submit($form, &$form_state) {
  if (!empty($form_state['values']['i18n_mode']) && $form_state['values']['i18n_mode'] == I18N_MODE_ENTITY_TRANSLATION) {
    $form_state['values']['entity_translation_taxonomy'] = TRUE;
  }
  $info = variable_get('entity_translation_taxonomy', array());
  $info[$form_state['vocabulary']->machine_name] = !empty($form_state['values']['entity_translation_taxonomy']);
  variable_set('entity_translation_taxonomy', $info);
}

/**
 * Implements hook_field_widget_WIDGET_TYPE_form_alter().
 */
function entity_translation_field_widget_taxonomy_autocomplete_form_alter(&$element, &$form_state, $context) {
  // Ensure autocomplete taxonomy terms are created in the language of the
  // parent entity.
  $field = field_info_field($element['#field_name']);
  if (!empty($field['settings']['allowed_values'][0]['vocabulary'])) {
    $bundle = $field['settings']['allowed_values'][0]['vocabulary'];
    if (entity_translation_enabled('taxonomy_term', $bundle)) {
      $element['#element_validate'][] = 'entity_translation_taxonomy_autocomplete_validate';
    }
  }
}

/**
 * Add the parent entity language to autocomplete taxonomy terms before they are
 * created.
 */
function entity_translation_taxonomy_autocomplete_validate($element, &$form_state) {
  if (!empty($form_state['values']['language'])) {
    // This is the language of the parent entity, that we will be appling to
    // terms.
    $language = $form_state['values']['language'];
    // Fetch the field values, this also works for nested values, such as field
    // collections.
    $value = drupal_array_get_nested_value($form_state['values'], $element['#parents']);
    foreach ($value as $delta => $term) {
      if ($term['tid'] = 'autocreate') {
        // Using the "autocreate" term created by taxonomy module, we create a
        // temporary object and initiate the original translation in the parent
        // entity language.
        $term = (object)$term;
        $handler = entity_translation_get_handler('taxonomy_term', $term);
        $handler->setOriginalLanguage($language);
        $handler->initTranslations();
        // Apply the language and translation structures to the original
        // "autocreate" term element.
        $value[$delta]['language'] = $language;
        $value[$delta]['translations'] = $term->translations;
      }
    }
    // Update form values.
    form_set_value($element, $value, $form_state);
  }
}
